import pytest

from hearth.agents.brain_mixed import MixedBrain
from hearth.agents.brain_scripted import ScriptedBrain
from hearth.agents.persona import personas
from hearth.config import Config
from hearth.sim.engine import Engine
from hearth.sim.events import EventKind
from hearth.world.actions import ActionType
from hearth.world.state import build_default_world


class EchoBrain(ScriptedBrain):
    """Stands in for a real model: replies with something that proves it saw the visitor's words."""
    async def converse(self, world, agent, persona, memory, history, visitor_text):
        return f"{persona.name} heard: {visitor_text} (turn {len(history) // 2 + 1})"


def make_engine():
    cfg = Config(tick_seconds=0, max_ticks=0, ws_enabled=False, voice_backend="none")
    world = build_default_world()
    brain = MixedBrain(default=ScriptedBrain(), overrides={"jonah": EchoBrain()})
    eng = Engine(cfg=cfg, world=world, brain=brain, ai_agents={"jonah"})
    for p in personas():
        eng.add_agent(p)
    return eng


@pytest.mark.asyncio
async def test_ai_flag_in_frames():
    eng = make_engine()
    init = eng.world_init_message()
    flags = {a["id"]: a["ai"] for a in init["agents"]}
    assert flags["jonah"] is True and flags["mara"] is False


@pytest.mark.asyncio
async def test_talk_routes_to_ai_brain_and_holds_agent():
    eng = make_engine()
    events = []
    eng.bus.subscribe(events.append)
    jonah = eng.agents["jonah"]
    reply = await eng.talk("Jonah", "Hey, what's going on here?")
    assert "heard: Hey, what's going on here?" in reply
    assert jonah.in_convo and jonah.state.current.type == ActionType.WAIT
    assert any(e.kind == EventKind.REPLY for e in events)
    assert any(e.kind == EventKind.SPEECH and e.agent == "jonah" for e in events)
    # Mara is at camp too, so she heard it
    assert any("heard:" in m for m in eng.agents["mara"].memory.episodic)
    reply2 = await eng.talk("jonah", "Need help?")
    assert "turn 2" in reply2
    await eng.end_talk("jonah")
    assert not jonah.in_convo and jonah.state.busy_until <= eng.world.clock.tick


@pytest.mark.asyncio
async def test_talk_to_scripted_agent_gets_canned_line():
    eng = make_engine()
    reply = await eng.talk("Mara", "Hello")
    assert reply and reply != "..."
