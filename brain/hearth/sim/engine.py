"""The tick loop. Owns the world, the agents' memories, and the brain; publishes events."""

from __future__ import annotations

import asyncio
import logging
import time
from dataclasses import dataclass, field
from typing import Any, Awaitable, Callable

from hearth.agents.brain_base import Brain
from hearth.agents.memory import Memory
from hearth.agents.perception import build_perception
from hearth.agents.persona import Persona
from hearth.agents.schema import Decision
from hearth.config import Config
from hearth.sim.events import Event, EventBus, EventKind
from hearth.world import rules
from hearth.world.actions import Action, ActionType, validate
from hearth.world.state import AgentState, World

log = logging.getLogger("hearth.engine")

INTERRUPTIBLE = {ActionType.WAIT, ActionType.REST}
WAIT_TICKS = 2  # a "wait" decision holds for this many ticks unless someone addresses you


@dataclass
class AgentRuntime:
    persona: Persona
    memory: Memory
    state: AgentState
    reflecting: asyncio.Task | None = None


@dataclass
class Engine:
    cfg: Config
    world: World
    brain: Brain
    bus: EventBus = field(default_factory=EventBus)
    snapshot_sinks: list[Callable[[dict[str, Any]], Awaitable[None] | None]] = field(default_factory=list)
    drain_hooks: list[Callable[[], Awaitable[None]]] = field(default_factory=list)  # e.g. wait for TTS
    agents: dict[str, AgentRuntime] = field(default_factory=dict)
    paused: bool = False
    stopped: bool = False

    # ------------------------------------------------------------------ setup
    def add_agent(self, persona: Persona) -> AgentRuntime:
        loc = self.world.locations[persona.start_location]
        st = AgentState(id=persona.id, name=persona.name, location=loc.id, x=loc.x, y=loc.y,
                        inventory=dict(persona.start_inventory))
        # small individual variation so needs don't all bottom out on the same tick
        r = self.world.rng
        st.needs.hunger -= r.uniform(0, 15)
        st.needs.thirst -= r.uniform(0, 15)
        st.needs.energy -= r.uniform(0, 10)
        self.world.agents[st.id] = st
        rt = AgentRuntime(persona=persona, state=st,
                          memory=Memory(window=self.cfg.episodic_window, reflect_every=self.cfg.reflect_every))
        self.agents[st.id] = rt
        return rt

    def world_init_message(self) -> dict[str, Any]:
        return {
            "type": "world_init",
            "locations": [loc.to_dict() for loc in self.world.locations.values()],
            "agents": [dict(a.state.to_dict(), voice=a.persona.voice) for a in self.agents.values()],
            "meters_to_units": 10,   # sim world is ~1 km across; Unreal map ~100 m
            "tick_seconds": self.cfg.tick_seconds,
            "travel_meters_per_tick": 400,
        }

    # ------------------------------------------------------------------ run
    async def run(self) -> None:
        await self.bus.publish(Event(kind=EventKind.SYSTEM, public=True, tick=0,
                                     text=f"{self.world.clock.label()}. {len(self.agents)} people at camp. The fire is out."))
        for rt in self.agents.values():
            rt.memory.remember(f"[{self.world.clock.label()}] We're all at camp. Nothing is built. The fire is out.")
        await self._emit_snapshot()

        while not self.stopped:
            if self.cfg.max_ticks is not None and self.world.clock.tick >= self.cfg.max_ticks:
                break
            if not self.world.living():
                await self.bus.publish(Event(kind=EventKind.SYSTEM, public=True, tick=self.world.clock.tick,
                                             text="Everyone is dead. The valley is quiet."))
                break
            if self.paused:
                await asyncio.sleep(0.2)
                continue
            t0 = time.monotonic()
            await self.tick()
            for hook in self.drain_hooks:
                await hook()
            if self.cfg.tick_seconds > 0:
                spent = time.monotonic() - t0
                await asyncio.sleep(max(0.0, self.cfg.tick_seconds - spent))

    async def tick(self) -> None:
        w = self.world
        w.clock.advance()
        tick = w.clock.tick
        events: list[Event] = []

        # 1-3 environment + bodies
        events += rules.roll_weather(w)
        rules.regenerate(w)
        events += rules.burn_fire(w)
        events += rules.decay_needs(w)

        # 4 resolve finished actions
        for rt in self.agents.values():
            st = rt.state
            if st.alive and st.current is not None and tick >= st.busy_until:
                events += rules.resolve_action(w, st)

        for e in events:
            e.tick = tick
        await self._publish_and_remember(events)

        # 5 who decides?
        deciders: list[AgentRuntime] = []
        for rt in self.agents.values():
            st = rt.state
            if not st.alive:
                continue
            interrupt = st.addressed_by and st.current is not None and st.current.type in INTERRUPTIBLE
            if st.is_idle(tick) or interrupt:
                if interrupt:
                    st.current = None
                deciders.append(rt)

        if deciders:
            perceptions = {rt.state.id: build_perception(w, rt.state, rt.memory).text for rt in deciders}
            results = await asyncio.gather(
                *(self.brain.decide(w, rt.state, rt.persona, rt.memory, perceptions[rt.state.id]) for rt in deciders),
                return_exceptions=True,
            )
            for rt, res in zip(deciders, results):
                if isinstance(res, Exception):
                    log.exception("brain failed for %s", rt.persona.name, exc_info=res)
                    res = Decision.wait(thought="(brain error)", plan=rt.memory.plan)
                await self._apply(rt, res)

        # 6 reflections in the background
        for rt in self.agents.values():
            if rt.state.alive and rt.memory.needs_reflection() and rt.reflecting is None:
                rt.reflecting = asyncio.create_task(self._reflect(rt))

        await self._emit_snapshot()

    # ------------------------------------------------------------------ decisions
    async def _apply(self, rt: AgentRuntime, d: Decision) -> None:
        w, st = self.world, rt.state
        tick = w.clock.tick
        st.last_decision_tick = tick
        st.addressed_by = []
        rt.memory.last_feedback = None
        rt.memory.plan = d.plan or rt.memory.plan
        out: list[Event] = []

        if d.thought:
            out.append(Event(kind=EventKind.THOUGHT, text=d.thought, agent=st.id, location=st.location, tick=tick))

        if d.say_text and d.say_text.strip():
            to_id = None
            if d.say_to:
                other = w.agent_by_name(d.say_to)
                if other and other.alive and other.location == st.location and other.moving_to is None and other.id != st.id:
                    to_id = other.id
                    other.addressed_by.append(st.name)
            st.speech_count += 1
            out.append(Event(kind=EventKind.SPEECH, text=d.say_text.strip(), agent=st.id, to=to_id,
                             location=st.location, tick=tick, extra={"voice": rt.persona.voice, "name": st.name}))

        action = Action(type=d.action_type, target=d.target, item=d.item, quantity=d.quantity)
        problem = validate(action, w, st)
        if problem:
            rt.memory.last_feedback = f"You tried to {action.type.value}" + (f" ({action.target})" if action.target else "") + f" but couldn't: {problem}"
            rt.memory.remember(f"[{w.clock.label()}] {rt.memory.last_feedback}")
            action = Action(type=ActionType.WAIT)
        ev = rules.start_action(w, st, action)
        if action.type == ActionType.WAIT:
            st.busy_until = tick + WAIT_TICKS
        if ev:
            ev.tick = tick
            out.append(ev)
        await self._publish_and_remember(out)

    async def _reflect(self, rt: AgentRuntime) -> None:
        try:
            notes = await self.brain.reflect(rt.persona, rt.memory)
            if notes:
                rt.memory.add_reflection(notes.strip())
                await self.bus.publish(Event(kind=EventKind.REFLECTION, text=notes.strip(), agent=rt.state.id,
                                             tick=self.world.clock.tick))
            else:
                rt.memory.since_reflection = 0
        except Exception:
            log.exception("reflection failed for %s", rt.persona.name)
            rt.memory.since_reflection = 0
        finally:
            rt.reflecting = None

    # ------------------------------------------------------------------ perception feed
    async def _publish_and_remember(self, events: list[Event]) -> None:
        for e in events:
            await self.bus.publish(e)
            if e.kind in (EventKind.THOUGHT, EventKind.REFLECTION):
                continue
            stamp = f"[{self.world.clock.label()}]"
            for rt in self.agents.values():
                st = rt.state
                if not st.alive:
                    continue
                hears = e.public or e.location is None or (st.location == e.location and st.moving_to is None)
                if e.kind == EventKind.SPEECH:
                    # a traveller who just left doesn't hear camp chatter
                    if not hears:
                        continue
                    if e.agent == st.id:
                        tgt = f" to {self.world.agents[e.to].name}" if e.to else ""
                        rt.memory.remember(f'{stamp} You said{tgt}: "{e.text}"')
                    else:
                        who = self.world.agents[e.agent].name
                        tgt = " to you" if e.to == st.id else (f" to {self.world.agents[e.to].name}" if e.to else "")
                        rt.memory.remember(f'{stamp} {who} said{tgt}: "{e.text}"')
                elif hears:
                    text = e.text
                    if e.agent == st.id and e.kind == EventKind.ACTION:
                        text = text.replace(st.name, "You", 1)
                    rt.memory.remember(f"{stamp} {text}")

    async def _emit_snapshot(self) -> None:
        snap = self.world.snapshot()
        for sink in self.snapshot_sinks:
            r = sink(snap)
            if asyncio.iscoroutine(r):
                await r

    # ------------------------------------------------------------------ god mode
    async def command(self, name: str, **kw: Any) -> str:
        w = self.world
        tick = w.clock.tick
        if name == "pause":
            self.paused = True
            return "paused"
        if name == "resume":
            self.paused = False
            return "resumed"
        if name == "storm":
            w.weather, w.weather_ticks_left = "storm", 12
            await self._publish_and_remember([Event(kind=EventKind.WEATHER, public=True, tick=tick,
                                                    text="A storm breaks over the valley — wind, driving rain, thunder.")])
            return "storm"
        if name == "drop_supplies":
            loc = w.location_by_name(kw.get("location", "camp")) or w.camp
            items = {k: int(v) for k, v in (kw.get("items") or {"berries": 6}).items()}
            store = loc.stockpile if loc.id == w.camp_id else loc.resources
            for k, v in items.items():
                store[k] = store.get(k, 0) + v
            desc = ", ".join(f"{v} {k}" for k, v in items.items())
            await self._publish_and_remember([Event(kind=EventKind.SYSTEM, location=loc.id, tick=tick,
                                                    text=f"Someone notices {desc} that wasn't there before, at the {loc.name}.")])
            return f"dropped {desc} at {loc.name}"
        if name == "whisper":
            st = w.agent_by_name(kw.get("agent", ""))
            if not st:
                return "no such agent"
            rt = self.agents[st.id]
            rt.memory.remember(f"[{w.clock.label()}] A thought surfaces, from nowhere: \"{kw.get('text', '')}\"")
            st.addressed_by.append("a voice in your head")
            return f"whispered to {st.name}"
        return f"unknown command {name}"
