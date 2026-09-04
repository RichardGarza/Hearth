"""Rule-based brain. No API. Exists so the whole pipeline (world, engine, voices, websocket, Unreal)
runs and can be tested without spending a token. It's deliberately simple but it does cooperate:
it announces intentions, replies when addressed, and shares food.
"""

from __future__ import annotations

import random

from hearth.agents.memory import Memory
from hearth.agents.persona import Persona
from hearth.agents.schema import Decision
from hearth.world.actions import ActionType
from hearth.world.state import FOOD, AgentState, World

# Loose role assignment so they don't all do the same thing.
ROLE_RESOURCE = {"mara": "wood", "jonah": "water", "teodora": "berries", "ravi": "fish", "lena": "berries", "oscar": "stone"}
RESOURCE_LOC = {"wood": "forest", "water": "river", "fish": "river", "berries": "meadow", "stone": "quarry"}

LINES = {
    "leave": ["Heading to the {loc} for {res}. Back before dark.", "I'll go get {res}. Anyone need anything from the {loc}?", "{res} run. Don't let the fire die while I'm out."],
    "return": ["Back. Got {n} {res}. Putting it in the pile.", "That's {n} {res} for the stockpile.", "Got the {res}. What's next?", "Made it back. {n} {res}."],
    "fire": ["Getting this fire going.", "Fire's low. On it.", "Putting wood on. It's going to be a cold one."],
    "build": ["Working on the shelter. Need more stone eventually.", "Shelter's coming along. Slowly.", "Give me an hour and this wall's up."],
    "hungry": ["Anyone got food? I'm running on empty.", "I need to eat something."],
    "reply": ["Yeah, I heard you, {who}.", "Okay, {who}.", "Got it, {who}. Doing that."],
    "give": ["Here, {who}. Eat.", "{who}, take this. You look rough."],
    "sleep": ["I'm done. Sleeping.", "Turning in. Wake me if the fire goes."],
    "night_worry": ["It's dark. Everyone here?", "Who's still out?"],
}


class ScriptedBrain:
    def __init__(self, seed: int = 7) -> None:
        self.rng = random.Random(seed)
        self._last: dict[str, str] = {}

    def _line(self, key: str, **kw) -> str:
        """Pick a line, avoiding the one most recently used for this key so two people
        deciding in the same tick don't say the exact same thing."""
        options = [l for l in LINES[key] if l != self._last.get(key)] or LINES[key]
        chosen = self.rng.choice(options)
        self._last[key] = chosen
        return chosen.format(**kw)

    async def decide(self, world: World, a: AgentState, persona: Persona, memory: Memory, perception: str) -> Decision:
        here = a.location
        at_camp = here == world.camp_id
        camp = world.camp
        n = a.needs
        say = None
        say_to = None
        people = world.agents_at(here, exclude=a.id)

        # reply if addressed
        if a.addressed_by and self.rng.random() < 0.8:
            who = a.addressed_by[-1]
            say, say_to = self._line("reply", who=who), who

        def dec(t: ActionType, target=None, item=None, quantity=None, thought="", plan=""):
            return Decision(thought=thought, say_text=say, say_to=say_to, action_type=t,
                            target=target, item=item, quantity=quantity, plan=plan)

        # 1. emergencies
        if n.thirst < 30:
            if here == "river" or a.inventory.get("water", 0) > 0 or (at_camp and camp.stockpile.get("water", 0) > 0):
                return dec(ActionType.DRINK, thought="thirsty")
            return dec(ActionType.MOVE_TO, "river", thought="need water")
        if n.hunger < 30:
            has = any(a.inventory.get(f, 0) > 0 for f in FOOD) or (at_camp and any(camp.stockpile.get(f, 0) > 0 for f in FOOD))
            if has:
                return dec(ActionType.EAT, thought="hungry")
            if say is None and people:
                say = self._line("hungry")
            if not at_camp:
                return dec(ActionType.MOVE_TO, "camp", thought="find food at camp")
            return dec(ActionType.MOVE_TO, "meadow", thought="find berries")

        # 2. share food with a hungry neighbour
        for p in people:
            if p.needs.hunger < 35:
                for f in FOOD:
                    if a.inventory.get(f, 0) > 1:
                        say, say_to = self._line("give", who=p.name), p.name
                        return dec(ActionType.GIVE, p.name, item=f, quantity=1, thought="share")

        # 3. night: go home, tend fire, sleep
        if world.clock.is_night or n.energy < 25:
            if not at_camp:
                return dec(ActionType.MOVE_TO, "camp", thought="night, go home")
            fire = camp.structures["fire"]
            if (not fire.lit or fire.fuel < 6) and (a.inventory.get("wood", 0) > 0 or camp.stockpile.get("wood", 0) > 0):
                if say is None:
                    say = self._line("fire")
                return dec(ActionType.TEND_FIRE, thought="fire")
            if any(v > 0 for v in a.inventory.values()):
                return dec(ActionType.DEPOSIT, thought="unload")
            if n.energy < 60:
                if say is None and self.rng.random() < 0.4:
                    say = self._line("sleep")
                return dec(ActionType.SLEEP, thought="sleep")
            if say is None and self.rng.random() < 0.2 and people:
                say = self._line("night_worry")
            return dec(ActionType.REST, thought="wait out the night")

        # 4. daytime: opportunistic drink at river
        if here == "river" and n.thirst < 80:
            return dec(ActionType.DRINK)

        # 5. carrying stuff -> go deposit
        carrying = sum(a.inventory.values())
        if carrying >= 6:
            if at_camp:
                res = max(a.inventory, key=lambda k: a.inventory[k])
                if say is None:
                    say = self._line("return", n=a.inventory[res], res=res)
                return dec(ActionType.DEPOSIT, thought="unload")
            return dec(ActionType.MOVE_TO, "camp", thought="bring haul home")

        # 6. camp chores
        if at_camp:
            fire = camp.structures["fire"]
            if not fire.lit and camp.stockpile.get("wood", 0) > 0:
                if say is None:
                    say = self._line("fire")
                return dec(ActionType.TEND_FIRE)
            sh = camp.structures["shelter"]
            if not sh.built and camp.stockpile.get("wood", 0) > 2 and camp.stockpile.get("stone", 0) > 0 and self.rng.random() < 0.6:
                if say is None:
                    say = self._line("build")
                return dec(ActionType.BUILD, "shelter")
            # go gather your role resource
            res = ROLE_RESOURCE.get(a.id, "wood")
            # if the stockpile is short on wood, everyone helps with wood
            if camp.stockpile.get("wood", 0) < 4 and self.rng.random() < 0.5:
                res = "wood"
            loc = RESOURCE_LOC[res]
            if say is None and people and self.rng.random() < 0.6:
                say = self._line("leave", loc=loc, res=res)
            return dec(ActionType.MOVE_TO, loc, thought=f"go get {res}")

        # 7. at a resource site: gather what's here
        loc = world.locations[here]
        options = [r for r, v in loc.resources.items() if v > 0]
        if here == "river":
            options.append("water")
        if options:
            res = ROLE_RESOURCE.get(a.id)
            if res not in options:
                res = self.rng.choice(options)
            return dec(ActionType.GATHER, res)
        return dec(ActionType.MOVE_TO, "camp", thought="nothing here")

    async def converse(self, world: World, agent: AgentState, persona: Persona, memory: Memory,
                       history: list[tuple[str, str]], visitor_text: str) -> str:
        worst, _ = agent.needs.worst()
        return self.rng.choice([
            f"Not now. I'm {worst.replace('energy', 'tired').replace('warmth', 'cold').replace('hunger', 'hungry').replace('thirst', 'thirsty')} and there's work.",
            "You're not from here, are you? Make yourself useful or stay out of the way.",
            "If you want to help, the forest's that way. Wood. Lots of it.",
        ])

    async def reflect(self, persona: Persona, memory: Memory) -> str | None:
        return None
