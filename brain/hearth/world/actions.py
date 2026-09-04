"""Actions an agent can take. Validation and durations live here; effects are applied in rules.py."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from hearth.world.state import World, AgentState


class ActionType(str, Enum):
    WAIT = "wait"            # stay put, listen, think (1 tick)
    MOVE_TO = "move_to"      # target = location name
    GATHER = "gather"        # target = resource at current location
    EAT = "eat"              # eat food from own inventory (or camp stockpile if at camp)
    DRINK = "drink"          # drink from river, own water, or camp water store
    REST = "rest"            # short rest, restores some energy
    SLEEP = "sleep"          # long rest; much better inside shelter near fire
    TEND_FIRE = "tend_fire"  # put wood on the fire / light it (camp only)
    BUILD = "build"          # target = "shelter"; consumes wood+stone from inventory or stockpile
    DEPOSIT = "deposit"      # put all carried resources into the camp stockpile
    TAKE = "take"            # target = resource, quantity from stockpile
    GIVE = "give"            # target = agent name, item + quantity from own inventory


ACTION_DESCRIPTIONS = {
    ActionType.WAIT: "Stay where you are for a few minutes. Use it to listen or wait for someone.",
    ActionType.MOVE_TO: "Walk to a location. target = location name.",
    ActionType.GATHER: "Gather a resource available where you stand. target = resource name. Takes ~30 min, yields 2-4.",
    ActionType.EAT: "Eat berries or fish from what you carry (or from the stockpile if at camp).",
    ActionType.DRINK: "Drink. Works at the river, or from water you carry, or the camp water store.",
    ActionType.REST: "Sit and rest ~30 min. Restores some energy.",
    ActionType.SLEEP: "Sleep for hours. Restores energy fully; safe and warm only with shelter and fire.",
    ActionType.TEND_FIRE: "At camp: light the fire or add wood to it. Uses wood you carry or from the stockpile. The fire keeps everyone warm at night.",
    ActionType.BUILD: "At camp: work on the shelter. target = 'shelter'. Each work session uses 1 wood + 1 stone and adds progress.",
    ActionType.DEPOSIT: "At camp: put everything you carry into the shared stockpile.",
    ActionType.TAKE: "At camp: take from the stockpile. target = resource, quantity = amount.",
    ActionType.GIVE: "Hand something you carry to a person at your location. target = person name, item + quantity.",
}


@dataclass
class Action:
    type: ActionType
    target: str | None = None
    item: str | None = None
    quantity: int | None = None
    started_tick: int = 0
    duration: int = 1

    def describe(self) -> str:
        t = self.type.value
        if self.type == ActionType.MOVE_TO:
            return f"walking to the {self.target}"
        if self.type == ActionType.GATHER:
            return f"gathering {self.target}"
        if self.type == ActionType.GIVE:
            return f"giving {self.quantity} {self.item} to {self.target}"
        if self.type == ActionType.TAKE:
            return f"taking {self.quantity} {self.target} from the stockpile"
        if self.type == ActionType.BUILD:
            return "working on the shelter"
        return t.replace("_", " ")


def duration_for(action: Action, world: "World", agent: "AgentState") -> int:
    """Ticks until the action resolves."""
    t = action.type
    if t == ActionType.MOVE_TO:
        return world.travel_ticks(agent.location, action.target or agent.location)
    if t == ActionType.GATHER:
        return 3
    if t == ActionType.REST:
        return 3
    if t == ActionType.SLEEP:
        return 36  # 6 hours
    if t == ActionType.BUILD:
        return 4
    return 1


def validate(action: Action, world: "World", agent: "AgentState") -> str | None:
    """Return None if valid, else a short reason the agent will be told."""
    from hearth.world.state import RESOURCES, FOOD

    t = action.type
    here = world.locations[agent.location]
    at_camp = agent.location == world.camp_id

    if t == ActionType.MOVE_TO:
        loc = world.location_by_name(action.target or "")
        if loc is None:
            return f"There is no place called '{action.target}'."
        if loc.id == agent.location:
            return f"You are already at the {loc.name}."
        action.target = loc.id
        return None

    if t == ActionType.GATHER:
        res = (action.target or "").lower()
        if res not in RESOURCES:
            return f"'{action.target}' is not something you can gather."
        if res == "water":
            if here.id != "river":
                return "Water can only be gathered at the river."
            return None
        if here.resources.get(res, 0) <= 0:
            return f"There is no {res} left to gather at the {here.name}."
        action.target = res
        return None

    if t == ActionType.EAT:
        carried = any(agent.inventory.get(f, 0) > 0 for f in FOOD)
        stock = at_camp and any(world.camp.stockpile.get(f, 0) > 0 for f in FOOD)
        if not carried and not stock:
            return "You have nothing to eat." + (" The stockpile has no food either." if at_camp else "")
        return None

    if t == ActionType.DRINK:
        if here.id == "river" or agent.inventory.get("water", 0) > 0:
            return None
        if at_camp and world.camp.stockpile.get("water", 0) > 0:
            return None
        return "There is no water here. Go to the river or carry some back."

    if t in (ActionType.TEND_FIRE, ActionType.BUILD, ActionType.DEPOSIT, ActionType.TAKE):
        if not at_camp:
            return "You have to be at camp for that."
        if t == ActionType.TEND_FIRE:
            if agent.inventory.get("wood", 0) <= 0 and world.camp.stockpile.get("wood", 0) <= 0:
                return "There is no wood to put on the fire. Someone needs to bring some from the forest."
        if t == ActionType.BUILD:
            sh = world.camp.structures["shelter"]
            if sh.built:
                return "The shelter is already finished."
            have_wood = agent.inventory.get("wood", 0) + world.camp.stockpile.get("wood", 0)
            have_stone = agent.inventory.get("stone", 0) + world.camp.stockpile.get("stone", 0)
            if have_wood < 1 or have_stone < 1:
                return "Building needs at least 1 wood and 1 stone available (carried or in the stockpile)."
        if t == ActionType.TAKE:
            res = (action.target or "").lower()
            if res not in RESOURCES:
                return f"'{action.target}' is not in the stockpile."
            if world.camp.stockpile.get(res, 0) <= 0:
                return f"The stockpile has no {res}."
            action.target = res
            action.quantity = max(1, min(action.quantity or 1, world.camp.stockpile[res]))
        if t == ActionType.DEPOSIT and not any(v > 0 for v in agent.inventory.values()):
            return "You are not carrying anything."
        return None

    if t == ActionType.GIVE:
        other = world.agent_by_name(action.target or "")
        if other is None or not other.alive:
            return f"There is no one called '{action.target}' here."
        if other.location != agent.location or other.moving_to is not None:
            return f"{other.name} is not here right now."
        item = (action.item or "").lower()
        if agent.inventory.get(item, 0) <= 0:
            return f"You are not carrying any {action.item}."
        action.target = other.id
        action.item = item
        action.quantity = max(1, min(action.quantity or 1, agent.inventory[item]))
        return None

    return None
