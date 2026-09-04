"""`python -m hearth run ...` — wire everything together and go."""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import signal
import sys
from datetime import datetime
from pathlib import Path

from hearth.agents.persona import personas
from hearth.config import Config
from hearth.sim.engine import Engine
from hearth.sim.events import Event, EventKind
from hearth.world.state import build_default_world

# ANSI colors for the console transcript
C = {"reset": "\033[0m", "dim": "\033[2m", "bold": "\033[1m",
     "mara": "\033[95m", "jonah": "\033[94m", "teodora": "\033[92m", "ravi": "\033[93m", "lena": "\033[96m", "oscar": "\033[91m"}


class ConsoleLog:
    def __init__(self, engine: Engine, show_thoughts: bool, show_actions: bool) -> None:
        self.engine = engine
        self.show_thoughts = show_thoughts
        self.show_actions = show_actions
        self._last_tick = -1

    def __call__(self, e: Event) -> None:
        w = self.engine.world
        if e.tick != self._last_tick:
            self._last_tick = e.tick
            fire = w.camp.structures["fire"]
            print(f"{C['dim']}── {w.clock.label()}  {w.weather}  fire:{'lit' if fire.lit else 'out'}{C['reset']}")
        name = w.agents[e.agent].name if e.agent else ""
        col = C.get(e.agent or "", "")
        if e.kind == EventKind.SPEECH:
            to = f" → {w.agents[e.to].name}" if e.to else ""
            print(f"  {col}{C['bold']}{name}{C['reset']}{col}{to}: {e.text}{C['reset']}")
        elif e.kind == EventKind.THOUGHT:
            if self.show_thoughts:
                print(f"  {C['dim']}({name} thinks: {e.text}){C['reset']}")
        elif e.kind == EventKind.REFLECTION:
            if self.show_thoughts:
                print(f"  {C['dim']}[{name} reflects]\n    " + e.text.replace("\n", "\n    ") + C["reset"])
        elif e.kind == EventKind.ACTION:
            if self.show_actions:
                print(f"  {C['dim']}· {e.text}{C['reset']}")
        else:
            print(f"  {C['bold']}* {e.text}{C['reset']}")


class JsonlLog:
    def __init__(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.f = path.open("a", encoding="utf-8")

    def __call__(self, e: Event) -> None:
        d = e.to_dict()
        d["kind"] = e.kind.value
        self.f.write(json.dumps(d, ensure_ascii=False) + "\n")
        self.f.flush()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="hearth", description="A world of people who must cooperate to survive.")
    sub = p.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run the simulation")
    r.add_argument("--brain", choices=["claude", "ollama", "scripted"], default="claude")
    r.add_argument("--voice", choices=["say", "none"], default="say")
    r.add_argument("--agents", type=int, default=6, help="how many of the 6 personas to spawn")
    r.add_argument("--ticks", type=int, default=None, help="stop after N ticks (default: run forever)")
    r.add_argument("--tick-seconds", type=float, default=None, help="real seconds per tick (default 3; 0 = max speed)")
    r.add_argument("--no-ws", action="store_true", help="don't start the WebSocket bridge")
    r.add_argument("--exit-with-client", action="store_true", help="stop when the game/viewer disconnects (used by play.sh)")
    r.add_argument("--wait-for-client", action="store_true", help="don't start the world until the game connects (used by play.sh)")
    r.add_argument("--quiet-start", type=int, default=None, help="ticks of silence after the game connects (default 6)")
    r.add_argument("--port", type=int, default=None)
    r.add_argument("--seed", type=int, default=None)
    r.add_argument("--model", default=None)
    r.add_argument("--effort", default=None, choices=["low", "medium", "high", "xhigh", "max"])
    r.add_argument("--budget", type=float, default=None, help="stop when estimated Claude spend reaches this many dollars")
    r.add_argument("--ollama-model", default=None, help="local model for --brain ollama (default qwen2.5:7b)")
    r.add_argument("--ai-agents", default=None, help="comma-separated ids that use --brain; everyone else is scripted (e.g. jonah)")
    r.add_argument("--thoughts", action="store_true", help="print agents' private thoughts")
    r.add_argument("--quiet", action="store_true", help="hide action lines, show only speech and events")
    r.add_argument("--no-sync-voice", action="store_true", help="don't pause the world while a line is being spoken")
    return p


async def run(args: argparse.Namespace) -> int:
    cfg = Config()
    if args.model:
        cfg.model = args.model
    if args.effort:
        cfg.effort = args.effort
    if args.tick_seconds is not None:
        cfg.tick_seconds = args.tick_seconds
    if args.ticks is not None:
        cfg.max_ticks = args.ticks
    if args.port:
        cfg.ws_port = args.port
    if args.seed is not None:
        cfg.seed = args.seed
    if args.budget is not None:
        cfg.budget_usd = args.budget
    if args.ollama_model:
        cfg.ollama_model = args.ollama_model
    cfg.voice_backend = args.voice
    cfg.ws_enabled = not args.no_ws
    cfg.exit_with_client = args.exit_with_client
    cfg.wait_for_client = args.wait_for_client
    if args.quiet_start is not None:
        cfg.quiet_start_ticks = args.quiet_start

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s", stream=sys.stderr)
    logging.getLogger("websockets").setLevel(logging.WARNING)
    logging.getLogger("httpx").setLevel(logging.WARNING)

    world = build_default_world(seed=cfg.seed, tick_minutes=cfg.tick_minutes)

    if args.brain == "claude":
        from hearth.agents.brain_claude import ClaudeBrain
        brain = ClaudeBrain(cfg)
        print(f"brain: claude ({cfg.model}, effort={cfg.effort}, fallbacks={'on' if cfg.fallbacks else 'off'}"
              + (f", budget=${cfg.budget_usd:.2f}" if cfg.budget_usd is not None else "") + ")")
    elif args.brain == "ollama":
        from hearth.agents.brain_ollama import OllamaBrain
        brain = OllamaBrain(cfg)
        print(f"brain: ollama ({cfg.ollama_model} at {cfg.ollama_url}, free)")
    else:
        from hearth.agents.brain_scripted import ScriptedBrain
        brain = ScriptedBrain(seed=cfg.seed)
        print("brain: scripted (no API calls)")

    ai_ids: set[str] = set()
    if args.ai_agents and args.brain != "scripted":
        from hearth.agents.brain_mixed import MixedBrain
        from hearth.agents.brain_scripted import ScriptedBrain
        ai_ids = {x.strip().lower() for x in args.ai_agents.split(",") if x.strip()}
        brain = MixedBrain(default=ScriptedBrain(seed=cfg.seed), overrides={i: brain for i in ai_ids})
        print(f"ai agents: {', '.join(sorted(ai_ids))} (others scripted)")
    elif args.brain != "scripted":
        ai_ids = {p.id for p in personas(args.agents)}

    engine = Engine(cfg=cfg, world=world, brain=brain, ai_agents=ai_ids)
    if cfg.wait_for_client and cfg.ws_enabled:
        engine.paused = True
        print("waiting for the game to connect before starting the world")
    for p in personas(args.agents):
        engine.add_agent(p)

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    engine.bus.subscribe(ConsoleLog(engine, show_thoughts=args.thoughts, show_actions=not args.quiet))
    engine.bus.subscribe(JsonlLog(cfg.log_dir / f"run-{stamp}.jsonl"))

    tts = None
    if cfg.voice_backend == "say":
        from hearth.voice.tts import SayBackend, SpeechQueue
        tts = SpeechQueue(SayBackend(rate=cfg.speech_rate))
        tts.start()
        engine.bus.subscribe(lambda e: None if engine.muted else tts.on_event(e))
        if not args.no_sync_voice:
            engine.drain_hooks.append(tts.drain)

    server = None
    if cfg.ws_enabled:
        from hearth.server.ws import BridgeServer
        server = BridgeServer(engine, cfg.ws_host, cfg.ws_port)
        try:
            await server.start()
            print(f"bridge: ws://{cfg.ws_host}:{cfg.ws_port}")
        except OSError as e:
            print(f"bridge: could not bind port {cfg.ws_port} ({e}); continuing without it")
            server = None

    loop = asyncio.get_running_loop()
    def _on_signal(name: str) -> None:
        logging.getLogger("hearth").info("received %s; stopping", name)
        engine.stopped = True
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        loop.add_signal_handler(sig, _on_signal, sig.name)

    print(f"log: {cfg.log_dir / f'run-{stamp}.jsonl'}\n")
    try:
        await engine.run()
    finally:
        if tts:
            await tts.drain()
            await tts.stop()
        if server:
            await server.stop()
        alive = [a.name for a in world.living()]
        print(f"\n{world.clock.label()} — alive: {', '.join(alive) or 'nobody'}")
        if hasattr(brain, "usage_line"):
            print("usage:", brain.usage_line())
    return 0


def main() -> None:
    args = build_parser().parse_args()
    if args.cmd == "run":
        sys.exit(asyncio.run(run(args)))


if __name__ == "__main__":
    main()
