"""World rules: what happens to bodies, weather, resources, and how actions resolve.

Everything here is pure world mutation and returns Event objects for the engine to publish.
"""

from __future__ import annotations

from hearth.sim.events import Event, EventKind
from hearth.world.actions import Action, ActionType
from hearth.world.state import FOOD, AgentState, World

# Needs decay per tick (10 world minutes). Tuned so an idle agent gets hungry in ~1.5 days,
# thirsty in ~1 day, exhausted after ~1 day awake, and cold within a few hours of a cold night.
BASE_DECAY = {"hunger": 0.45, "thirst": 0.7, "energy": 0.35, "warmth": 0.0}
WORKING_MULT = {"hunger": 1.6, "thirst": 1.6, "energy": 2.2}

WEATHER_TABLE = [("clear", 0.45), ("overcast", 0.25), ("rain", 0.18), ("cold", 0.08), ("storm", 0.04)]


# ---------------------------------------------------------------- weather

def roll_weather(world: World) -> list[Event]:
    events: list[Event] = []
    if world.weather_ticks_left > 0:
        world.weather_ticks_left -= 1
        return events
    r = world.rng.random()
    acc = 0.0
    new = "clear"
    for name, p in WEATHER_TABLE:
        acc += p
        if r <= acc:
            new = name
            break
    world.weather_ticks_left = world.rng.randint(6, 24)  # 1-4 hours
    if new != world.weather:
        world.weather = new
        text = {
            "clear": "The sky clears.",
            "overcast": "Clouds roll in and the light goes grey.",
            "rain": "Rain starts falling.",
            "cold": "A cold wind picks up. It bites.",
            "storm": "A storm breaks over the valley — wind, driving rain, thunder.",
        }[new]
        events.append(Event(kind=EventKind.WEATHER, text=text, location=None, public=True))
    return events


def cold_pressure(world: World) -> float:
    """Warmth lost per tick from environment, before fire/shelter."""
    base = 0.0
    if world.clock.is_night:
        base += 1.2
    base += {"clear": 0.0, "overcast": 0.2, "rain": 0.9, "cold": 1.6, "storm": 1.8}[world.weather]
    return base


# ---------------------------------------------------------------- resources

def regenerate(world: World) -> None:
    for loc in world.locations.values():
        for res, rate in loc.regen.items():
            if rate <= 0:
                continue
            loc._regen_acc[res] = loc._regen_acc.get(res, 0.0) + rate
            whole = int(loc._regen_acc[res])
            if whole:
                loc._regen_acc[res] -= whole
                cap = loc.max_resources.get(res, 999)
                loc.resources[res] = min(cap, loc.resources.get(res, 0) + whole)


def burn_fire(world: World) -> list[Event]:
    fire = world.camp.structures["fire"]
    if fire.lit:
        fire.fuel -= 1
        if fire.fuel <= 0:
            fire.lit = False
            fire.fuel = 0
            return [Event(kind=EventKind.FIRE_OUT, text="The fire at camp has gone out.",
                          location=world.camp_id, public=True)]
    return []


# ---------------------------------------------------------------- bodies

def decay_needs(world: World) -> list[Event]:
    events: list[Event] = []
    fire = world.camp.structures["fire"]
    shelter = world.camp.structures["shelter"]
    cold = cold_pressure(world)

    for a in world.living():
        n = a.needs
        working = a.current is not None and a.current.type in (
            ActionType.GATHER, ActionType.BUILD, ActionType.MOVE_TO)
        sleeping = a.current is not None and a.current.type == ActionType.SLEEP
        resting = a.current is not None and a.current.type == ActionType.REST

        for k, base in BASE_DECAY.items():
            if k == "warmth":
                continue
            rate = base
            if working:
                rate *= WORKING_MULT[k]
            if k == "energy":
                if sleeping:
                    rate = -2.6   # ~+94 over 6h
                elif resting:
                    rate = -1.5
            setattr(n, k, getattr(n, k) - rate)

        # warmth
        at_camp = a.location == world.camp_id and a.moving_to is None
        warm_gain = 0.0
        if at_camp and fire.lit:
            warm_gain += 2.0
        if at_camp and shelter.built:
            cold *= 0.35
        if working:
            warm_gain += 0.4
        n.warmth += warm_gain - cold
        n.clamp()

        # health
        dmg = 0.0
        if n.hunger <= 0:
            dmg += 1.0
        if n.thirst <= 0:
            dmg += 2.0
        if n.warmth <= 0:
            dmg += 2.0
        if n.energy <= 0:
            dmg += 0.4
        if dmg == 0 and n.hunger > 30 and n.thirst > 30 and n.warmth > 30:
            n.health = min(100.0, n.health + 0.5)
        else:
            n.health -= dmg
        n.clamp()

        if n.health <= 0:
            a.alive = False
            worst = "thirst" if n.thirst <= 0 else "cold" if n.warmth <= 0 else "hunger" if n.hunger <= 0 else "exhaustion"
            a.cause_of_death = worst
            a.current = None
            events.append(Event(kind=EventKind.DEATH, text=f"{a.name} has died of {worst}.",
                                location=a.location, agent=a.id, public=True))

    return events


# ---------------------------------------------------------------- actions

def start_action(world: World, agent: AgentState, action: Action) -> Event | None:
    """Begin an action (already validated). Returns an event describing it, if visible."""
    from hearth.world.actions import duration_for

    action.started_tick = world.clock.tick
    action.duration = duration_for(action, world, agent)
    agent.current = action
    agent.busy_until = world.clock.tick + action.duration

    if action.type == ActionType.MOVE_TO:
        agent.moving_to = action.target
        dest = world.locations[action.target]
        return Event(kind=EventKind.ACTION, text=f"{agent.name} sets off toward the {dest.name}.",
                     location=agent.location, agent=agent.id)
    if action.type == ActionType.WAIT:
        return None
    return Event(kind=EventKind.ACTION, text=f"{agent.name} starts {action.describe()}.",
                 location=agent.location, agent=agent.id)


def resolve_action(world: World, agent: AgentState) -> list[Event]:
    """Apply the effect of agent.current, which has just finished."""
    action = agent.current
    agent.current = None
    if action is None or not agent.alive:
        return []
    ev: list[Event] = []
    here = world.locations[agent.location]
    camp = world.camp
    t = action.type

    def add(text: str, kind: EventKind = EventKind.ACTION, loc: str | None = None, public: bool = False):
        ev.append(Event(kind=kind, text=text, location=loc or agent.location, agent=agent.id, public=public))

    if t == ActionType.MOVE_TO:
        dest = world.locations[action.target]
        agent.location = dest.id
        agent.x, agent.y = dest.x, dest.y
        agent.moving_to = None
        others = [o.name for o in world.agents_at(dest.id, exclude=agent.id)]
        who = f" {', '.join(others)} {'is' if len(others) == 1 else 'are'} here." if others else " No one else is here."
        add(f"{agent.name} arrives at the {dest.name}.{who}")

    elif t == ActionType.GATHER:
        res = action.target
        if res == "water":
            got = 4
        else:
            got = min(here.resources.get(res, 0), world.rng.randint(2, 4))
            here.resources[res] = here.resources.get(res, 0) - got
        agent.inventory[res] = agent.inventory.get(res, 0) + got
        add(f"{agent.name} gathered {got} {res}. (carrying {agent.inventory[res]})")

    elif t == ActionType.EAT:
        src = None
        for f in FOOD:
            if agent.inventory.get(f, 0) > 0:
                src, store = f, agent.inventory
                break
        if src is None and agent.location == world.camp_id:
            for f in FOOD:
                if camp.stockpile.get(f, 0) > 0:
                    src, store = f, camp.stockpile
                    break
        if src is None:
            add(f"{agent.name} finds nothing to eat.")
        else:
            store[src] -= 1
            agent.needs.hunger = min(100.0, agent.needs.hunger + (30 if src == "berries" else 45))
            add(f"{agent.name} eats {src}.")

    elif t == ActionType.DRINK:
        if here.id == "river":
            agent.needs.thirst = 100.0
            add(f"{agent.name} drinks from the river.")
        elif agent.inventory.get("water", 0) > 0:
            agent.inventory["water"] -= 1
            agent.needs.thirst = min(100.0, agent.needs.thirst + 35)
            add(f"{agent.name} drinks from the water they carry.")
        elif agent.location == world.camp_id and camp.stockpile.get("water", 0) > 0:
            camp.stockpile["water"] -= 1
            agent.needs.thirst = min(100.0, agent.needs.thirst + 35)
            add(f"{agent.name} drinks from the camp water store.")
        else:
            add(f"{agent.name} finds no water.")

    elif t == ActionType.REST:
        add(f"{agent.name} finishes resting.")

    elif t == ActionType.SLEEP:
        add(f"{agent.name} wakes up.")

    elif t == ActionType.TEND_FIRE:
        fire = camp.structures["fire"]
        wood_from = agent.inventory if agent.inventory.get("wood", 0) > 0 else camp.stockpile
        use = min(3, wood_from.get("wood", 0))
        wood_from["wood"] -= use
        fire.fuel += use * 6  # each wood = 1 hour
        was_lit = fire.lit
        fire.lit = fire.fuel > 0
        if not was_lit and fire.lit:
            add(f"{agent.name} lights the fire. It will burn about {fire.fuel * world.clock.tick_minutes // 60} hours on what's there.",
                kind=EventKind.FIRE_LIT, public=True)
        else:
            add(f"{agent.name} adds {use} wood to the fire (about {fire.fuel * world.clock.tick_minutes // 60} hours of fuel).")

    elif t == ActionType.BUILD:
        sh = camp.structures["shelter"]
        def take(res: str) -> bool:
            if agent.inventory.get(res, 0) > 0:
                agent.inventory[res] -= 1
                return True
            if camp.stockpile.get(res, 0) > 0:
                camp.stockpile[res] -= 1
                return True
            return False
        if take("wood") and take("stone"):
            sh.progress += 1
            if sh.progress >= sh.required:
                sh.built = True
                add(f"{agent.name} finishes the shelter. Camp now has cover from rain and cold.",
                    kind=EventKind.STRUCTURE_BUILT, public=True)
            else:
                add(f"{agent.name} works on the shelter ({sh.progress}/{sh.required} done).")
        else:
            add(f"{agent.name} ran out of wood or stone before finishing the work session.")

    elif t == ActionType.DEPOSIT:
        moved = []
        for res, qty in list(agent.inventory.items()):
            if qty > 0:
                camp.stockpile[res] = camp.stockpile.get(res, 0) + qty
                moved.append(f"{qty} {res}")
                agent.inventory[res] = 0
        add(f"{agent.name} puts {', '.join(moved)} into the stockpile.")

    elif t == ActionType.TAKE:
        res, q = action.target, action.quantity or 1
        q = min(q, camp.stockpile.get(res, 0))
        camp.stockpile[res] -= q
        agent.inventory[res] = agent.inventory.get(res, 0) + q
        add(f"{agent.name} takes {q} {res} from the stockpile.")

    elif t == ActionType.GIVE:
        other = world.agents[action.target]
        q = min(action.quantity or 1, agent.inventory.get(action.item, 0))
        if other.alive and other.location == agent.location and q > 0:
            agent.inventory[action.item] -= q
            other.inventory[action.item] = other.inventory.get(action.item, 0) + q
            add(f"{agent.name} gives {q} {action.item} to {other.name}.")
        else:
            add(f"{agent.name} tried to give {action.item} to {other.name}, but couldn't.")

    elif t == ActionType.WAIT:
        pass

    return ev
