"""Build what an agent perceives at decision time: body, surroundings, people, memory."""

from __future__ import annotations

from dataclasses import dataclass

from hearth.agents.memory import Memory
from hearth.world.state import AgentState, World


def _needs_words(v: float, good: str, mid: str, bad: str, crit: str) -> str:
    if v > 65:
        return good
    if v > 40:
        return mid
    if v > 15:
        return bad
    return crit


def body_report(a: AgentState) -> str:
    n = a.needs
    parts = [
        _needs_words(n.hunger, "not hungry", "getting hungry", "very hungry", "starving"),
        _needs_words(n.thirst, "not thirsty", "getting thirsty", "very thirsty", "dangerously dehydrated"),
        _needs_words(n.energy, "rested", "a bit tired", "exhausted", "about to collapse"),
        _needs_words(n.warmth, "warm", "a little cold", "very cold", "freezing"),
    ]
    health = "healthy" if n.health > 70 else "unwell" if n.health > 35 else "badly weakened"
    return f"You feel {', '.join(parts)}. Overall you are {health}. " \
           f"(hunger {n.hunger:.0f}/100, thirst {n.thirst:.0f}/100, energy {n.energy:.0f}/100, warmth {n.warmth:.0f}/100, health {n.health:.0f}/100)"


@dataclass
class Perception:
    text: str


def build_perception(world: World, a: AgentState, mem: Memory) -> Perception:
    here = world.locations[a.location]
    camp = world.camp
    fire = camp.structures["fire"]
    shelter = camp.structures["shelter"]
    lines: list[str] = []

    lines.append(f"TIME: {world.clock.label()}{' (night)' if world.clock.is_night else ''}. WEATHER: {world.weather}.")
    lines.append(f"YOU ARE AT: the {here.name}. {here.description}")

    # resources here
    res = {k: v for k, v in here.resources.items() if v > 0}
    if here.id == "river":
        res["water"] = "plenty"
    lines.append("AVAILABLE HERE: " + (", ".join(f"{k} ({v})" for k, v in res.items()) if res else "nothing to gather"))

    # people here
    people = world.agents_at(here.id, exclude=a.id)
    if people:
        descs = []
        for p in people:
            doing = p.current.describe() if p.current else "idle"
            descs.append(f"{p.name} ({doing})")
        lines.append("PEOPLE HERE: " + ", ".join(descs))
    else:
        lines.append("PEOPLE HERE: no one else.")

    # others elsewhere (what you'd know from having seen them leave)
    away = [f"{p.name} → {world.locations[p.moving_to].name if p.moving_to else world.locations[p.location].name}"
            for p in world.living() if p.id != a.id and p not in people]
    if away:
        lines.append("OTHERS, LAST KNOWN: " + "; ".join(away))
    dead = [p.name for p in world.agents.values() if not p.alive]
    if dead:
        lines.append("DEAD: " + ", ".join(dead))

    # camp status (everyone knows the camp; it's home)
    fire_s = f"lit, ~{fire.fuel * world.clock.tick_minutes // 60}h of wood on it" if fire.lit else "OUT"
    sh_s = "built" if shelter.built else f"not built ({shelter.progress}/{shelter.required} work sessions done, each needs 1 wood + 1 stone)"
    stock = ", ".join(f"{k} {v}" for k, v in camp.stockpile.items() if v > 0) or "empty"
    lines.append(f"CAMP: fire {fire_s}. Shelter {sh_s}. Stockpile: {stock}.")

    # body + inventory
    lines.append("BODY: " + body_report(a))
    inv = ", ".join(f"{k} {v}" for k, v in a.inventory.items() if v > 0) or "nothing"
    lines.append(f"CARRYING: {inv}.")

    if mem.last_feedback:
        lines.append(f"NOTE: {mem.last_feedback}")

    lines.append("")
    lines.append("YOUR LONG-TERM NOTES:\n" + mem.reflections_text())
    lines.append("")
    lines.append("RECENT (what you saw and heard, oldest first):\n" + mem.recent_text())
    lines.append("")
    lines.append(f"YOUR LAST PLAN NOTE: {mem.plan or '(none)'}")
    if a.addressed_by:
        lines.append(f"\n{', '.join(a.addressed_by)} just spoke to you directly. Respond to them if it makes sense.")

    lines.append("\nDecide what to do now.")
    return Perception(text="\n".join(lines))
