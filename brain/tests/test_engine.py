import pytest

from hearth.agents.brain_scripted import ScriptedBrain
from hearth.agents.persona import personas
from hearth.agents.schema import Decision
from hearth.config import Config
from hearth.sim.engine import Engine
from hearth.sim.events import EventKind
from hearth.world.actions import ActionType
from hearth.world.state import build_default_world


def make_engine(ticks=200, seed=7):
    cfg = Config(tick_seconds=0, max_ticks=ticks, seed=seed, ws_enabled=False, voice_backend="none")
    world = build_default_world(seed=seed)
    eng = Engine(cfg=cfg, world=world, brain=ScriptedBrain(seed=seed))
    for p in personas():
        eng.add_agent(p)
    return eng


@pytest.mark.asyncio
async def test_scripted_world_runs_and_people_talk():
    eng = make_engine(ticks=300)
    events = []
    eng.bus.subscribe(events.append)
    await eng.run()
    kinds = {e.kind for e in events}
    assert EventKind.SPEECH in kinds
    assert EventKind.FIRE_LIT in kinds
    assert eng.world.clock.tick == 300
    # nobody should die of neglect in the first ~2 days under the scripted brain
    assert len(eng.world.living()) >= 5, [a.cause_of_death for a in eng.world.agents.values()]


@pytest.mark.asyncio
async def test_speech_is_heard_only_at_same_location():
    eng = make_engine(ticks=0)
    mara, ravi = eng.agents["mara"], eng.agents["ravi"]
    ravi.state.location = "river"
    await eng._apply(mara, Decision(thought="", say_text="Anyone there?", say_to=None, action_type=ActionType.WAIT))
    assert any("Anyone there?" in m for m in eng.agents["jonah"].memory.episodic)
    assert not any("Anyone there?" in m for m in ravi.memory.episodic)


@pytest.mark.asyncio
async def test_addressed_agent_is_flagged_and_interrupted():
    eng = make_engine(ticks=0)
    mara, jonah = eng.agents["mara"], eng.agents["jonah"]
    await eng._apply(jonah, Decision(thought="", say_text=None, say_to=None, action_type=ActionType.REST))
    assert jonah.state.current is not None
    await eng._apply(mara, Decision(thought="", say_text="Jonah, get water.", say_to="Jonah", action_type=ActionType.WAIT))
    assert jonah.state.addressed_by == ["Mara"]
    assert any('Mara said to you: "Jonah, get water."' in m for m in jonah.memory.episodic)


@pytest.mark.asyncio
async def test_invalid_action_becomes_wait_with_feedback():
    eng = make_engine(ticks=0)
    mara = eng.agents["mara"]
    await eng._apply(mara, Decision(thought="", say_text=None, say_to=None, action_type=ActionType.MOVE_TO, target="Moon"))
    assert mara.state.current.type == ActionType.WAIT
    assert "no place" in mara.memory.last_feedback


@pytest.mark.asyncio
async def test_god_commands():
    eng = make_engine(ticks=0)
    assert await eng.command("storm") == "storm"
    assert eng.world.weather == "storm"
    assert "dropped" in await eng.command("drop_supplies", location="camp", items={"fish": 3})
    assert eng.world.camp.stockpile["fish"] == 3
    assert "whispered" in await eng.command("whisper", agent="Lena", text="Check on Teodora.")
    assert eng.agents["lena"].state.addressed_by


def test_decision_from_json_tolerates_garbage():
    d = Decision.from_json({"thought": "x", "say": None, "action": {"type": "fly", "target": None, "item": None, "quantity": None}, "plan": ""})
    assert d.action_type == ActionType.WAIT
    d = Decision.from_json({"thought": "x", "say": {"to": "Mara", "text": "hi"}, "action": {"type": "move_to", "target": "River", "item": None, "quantity": None}, "plan": "go"})
    assert d.say_to == "Mara" and d.target == "River" and d.action_type == ActionType.MOVE_TO


@pytest.mark.asyncio
async def test_speech_is_throttled_unless_addressed():
    eng = make_engine(ticks=0)
    mara, jonah = eng.agents["mara"], eng.agents["jonah"]
    events = []
    eng.bus.subscribe(events.append)
    await eng._apply(mara, Decision(thought="", say_text="One.", say_to=None, action_type=ActionType.WAIT))
    await eng._apply(mara, Decision(thought="", say_text="Two.", say_to=None, action_type=ActionType.WAIT))
    lines = [e.text for e in events if e.kind == EventKind.SPEECH]
    assert lines == ["One."]                       # second line dropped: too soon, nobody asked
    await eng._apply(jonah, Decision(thought="", say_text="Mara?", say_to="Mara", action_type=ActionType.WAIT))
    await eng._apply(mara, Decision(thought="", say_text="Yes, Jonah.", say_to="Jonah", action_type=ActionType.WAIT))
    lines = [e.text for e in events if e.kind == EventKind.SPEECH]
    assert lines[-1] == "Yes, Jonah."              # replying to someone bypasses the throttle


@pytest.mark.asyncio
async def test_quiet_start_and_mute_commands():
    eng = make_engine(ticks=0)
    eng.cfg.wait_for_client = True
    eng.paused = True
    assert await eng.command("viewer_ready") == "started"
    assert not eng.paused and eng.quiet_until_tick == eng.cfg.quiet_start_ticks
    events = []
    eng.bus.subscribe(events.append)
    mara = eng.agents["mara"]
    await eng._apply(mara, Decision(thought="", say_text="Too early.", say_to=None, action_type=ActionType.WAIT))
    assert not [e for e in events if e.kind == EventKind.SPEECH]
    eng.quiet_until_tick = 0
    await eng._apply(mara, Decision(thought="", say_text="Now.", say_to=None, action_type=ActionType.WAIT))
    assert [e.text for e in events if e.kind == EventKind.SPEECH] == ["Now."]
    assert await eng.command("mute") == "muted" and eng.muted
    assert await eng.command("unmute") == "unmuted" and not eng.muted
