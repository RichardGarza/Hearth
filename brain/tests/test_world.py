from hearth.world import rules
from hearth.world.actions import Action, ActionType, validate
from hearth.world.state import AgentState, build_default_world


def make_agent(world, id="a", name="A", loc="camp"):
    L = world.locations[loc]
    st = AgentState(id=id, name=name, location=loc, x=L.x, y=L.y)
    world.agents[id] = st
    return st


def test_clock_labels_and_night():
    w = build_default_world()
    assert w.clock.label() == "Day 1, 06:00"
    assert not w.clock.is_night
    for _ in range(6 * 16):  # +16h -> 22:00
        w.clock.advance()
    assert w.clock.hour == 22 and w.clock.is_night


def test_needs_decay_and_death():
    w = build_default_world()
    a = make_agent(w)
    a.needs.thirst = 1
    a.needs.health = 3
    for _ in range(5):
        rules.decay_needs(w)
    assert not a.alive and a.cause_of_death == "thirst"


def test_fire_warms_at_night():
    w = build_default_world()
    a = make_agent(w)
    w.clock.minutes = 23 * 60
    a.needs.warmth = 50
    rules.decay_needs(w)
    cold = a.needs.warmth
    a.needs.warmth = 50
    w.camp.structures["fire"].lit = True
    w.camp.structures["fire"].fuel = 10
    rules.decay_needs(w)
    assert a.needs.warmth > cold


def test_gather_then_deposit_then_tend_fire():
    w = build_default_world()
    a = make_agent(w, loc="forest")
    act = Action(type=ActionType.GATHER, target="wood")
    assert validate(act, w, a) is None
    rules.start_action(w, a, act)
    w.clock.tick = a.busy_until
    rules.resolve_action(w, a)
    assert a.inventory["wood"] >= 2

    mv = Action(type=ActionType.MOVE_TO, target="Camp")
    assert validate(mv, w, a) is None
    rules.start_action(w, a, mv)
    w.clock.tick = a.busy_until
    rules.resolve_action(w, a)
    assert a.location == "camp"

    tf = Action(type=ActionType.TEND_FIRE)
    assert validate(tf, w, a) is None
    rules.start_action(w, a, tf)
    w.clock.tick = a.busy_until
    evs = rules.resolve_action(w, a)
    assert w.camp.structures["fire"].lit
    assert any(e.kind.value == "fire_lit" for e in evs)


def test_validation_messages():
    w = build_default_world()
    a = make_agent(w)
    assert "no place" in validate(Action(type=ActionType.MOVE_TO, target="Mars"), w, a)
    assert "already" in validate(Action(type=ActionType.MOVE_TO, target="camp"), w, a)
    assert validate(Action(type=ActionType.BUILD, target="shelter"), w, a) is not None
    assert "nothing to eat" in validate(Action(type=ActionType.EAT), w, a)
    a.location = "forest"
    assert "at camp" in validate(Action(type=ActionType.DEPOSIT), w, a)


def test_give_between_agents():
    w = build_default_world()
    a = make_agent(w, "a", "A")
    b = make_agent(w, "b", "B")
    a.inventory["berries"] = 3
    g = Action(type=ActionType.GIVE, target="B", item="berries", quantity=2)
    assert validate(g, w, a) is None
    rules.start_action(w, a, g)
    w.clock.tick = a.busy_until
    rules.resolve_action(w, a)
    assert b.inventory["berries"] == 2 and a.inventory["berries"] == 1


def test_shelter_build_completes():
    w = build_default_world()
    a = make_agent(w)
    w.camp.stockpile.update({"wood": 10, "stone": 10})
    for _ in range(8):
        act = Action(type=ActionType.BUILD, target="shelter")
        assert validate(act, w, a) is None
        rules.start_action(w, a, act)
        w.clock.tick = a.busy_until
        rules.resolve_action(w, a)
    assert w.camp.structures["shelter"].built
    assert w.camp.stockpile["wood"] == 2
