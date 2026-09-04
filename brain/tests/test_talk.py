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


def test_detic_strips_repeated_openers():
    from hearth.agents.brain_ollama import detic
    assert detic("Hey, thanks! We need wood.", None, first=True) == "Hey, thanks! We need wood."
    assert detic("Hey, thanks! We need wood.", "Hey, thanks for asking.", first=False) == "We need wood."
    assert detic("Wait, how about the river?", "Wait, how do we start?", first=False) == "How about the river?"
    assert detic("Sure. The forest is east.", "Nope.", first=False) == "Sure. The forest is east."


@pytest.mark.asyncio
async def test_visitor_gathers_and_drops_off():
    eng = make_engine()
    fish_before = eng.world.locations["river"].resources["fish"]
    assert await eng.visitor_gather("river") == "fish"
    assert await eng.visitor_gather("River") == "fish"
    assert eng.visitor_inventory == {"fish": 2}
    assert eng.world.locations["river"].resources["fish"] == fish_before - 2
    assert await eng.visitor_gather("camp") is None
    moved = await eng.visitor_deposit()
    assert moved == {"fish": 2} and eng.visitor_inventory == {}
    assert eng.world.camp.stockpile["fish"] == 2
    assert "Dropped off" in eng.visitor_state()["last"]
    assert any("visitor puts 2 fish" in m for m in eng.agents["mara"].memory.episodic)


def test_prompts_load_from_files():
    from hearth.agents.prompts import WORLD_RULES, CONVERSE_RULES, LOCAL_MODEL_HINT, PROMPT_DIR
    assert (PROMPT_DIR / "world_rules.md").exists()
    assert "- move_to:" in WORLD_RULES and "{ACTIONS}" not in WORLD_RULES and "PLACES:" in WORLD_RULES
    assert CONVERSE_RULES and LOCAL_MODEL_HINT
    from hearth.agents.persona import PERSONAS
    assert [p.id for p in PERSONAS] == ["mara", "jonah", "teodora", "ravi", "lena", "oscar"]
    assert PERSONAS[0].body == "quinn" and PERSONAS[1].body == "manny"
