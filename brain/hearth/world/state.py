"""World state: the single source of truth the engine mutates and everyone else reads."""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field
from typing import Any

from hearth.world.actions import Action

# Resource ids. Keep this list short; the agents see it verbatim.
RESOURCES = ("wood", "water", "berries", "fish", "stone")
FOOD = ("berries", "fish")


@dataclass
class Clock:
    tick: int = 0
    minutes: int = 6 * 60  # world starts at 06:00 on day 1
    tick_minutes: int = 10

    @property
    def day(self) -> int:
        return self.minutes // (24 * 60) + 1

    @property
    def hour(self) -> int:
        return (self.minutes // 60) % 24

    @property
    def minute(self) -> int:
        return self.minutes % 60

    @property
    def is_night(self) -> bool:
        return self.hour >= 21 or self.hour < 6

    def advance(self) -> None:
        self.tick += 1
        self.minutes += self.tick_minutes

    def label(self) -> str:
        return f"Day {self.day}, {self.hour:02d}:{self.minute:02d}"

    def to_dict(self) -> dict[str, Any]:
        return {"day": self.day, "hour": self.hour, "minute": self.minute, "is_night": self.is_night}


@dataclass
class Structure:
    kind: str                 # "fire" | "shelter" | "water_store"
    built: bool = False
    lit: bool = False         # fire only
    fuel: int = 0             # fire only: ticks of burn remaining
    progress: int = 0         # build progress 0..required
    required: int = 0

    def to_dict(self) -> dict[str, Any]:
        return {"built": self.built, "lit": self.lit, "fuel": self.fuel,
                "progress": self.progress, "required": self.required}


@dataclass
class Location:
    id: str
    name: str
    x: float
    y: float
    description: str
    resources: dict[str, int] = field(default_factory=dict)
    max_resources: dict[str, int] = field(default_factory=dict)
    regen: dict[str, float] = field(default_factory=dict)   # units per tick (fractional accumulates)
    _regen_acc: dict[str, float] = field(default_factory=dict)
    structures: dict[str, Structure] = field(default_factory=dict)
    stockpile: dict[str, int] = field(default_factory=dict)  # shared storage, camp only

    def distance_to(self, other: "Location") -> float:
        return math.hypot(self.x - other.x, self.y - other.y)

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id, "name": self.name, "x": self.x, "y": self.y,
            "resources": dict(self.resources),
            "structures": {k: v.to_dict() for k, v in self.structures.items()},
            "stockpile": dict(self.stockpile),
        }


@dataclass
class Needs:
    """All 0..100. 100 is good. Health drops when the others bottom out."""
    hunger: float = 80.0     # 100 = full
    thirst: float = 80.0     # 100 = hydrated
    energy: float = 90.0     # 100 = rested
    warmth: float = 80.0     # 100 = warm
    health: float = 100.0

    def clamp(self) -> None:
        for k in ("hunger", "thirst", "energy", "warmth", "health"):
            setattr(self, k, max(0.0, min(100.0, getattr(self, k))))

    def to_dict(self) -> dict[str, int]:
        return {k: int(round(getattr(self, k))) for k in ("hunger", "thirst", "energy", "warmth", "health")}

    def worst(self) -> tuple[str, float]:
        items = [(k, getattr(self, k)) for k in ("hunger", "thirst", "energy", "warmth")]
        return min(items, key=lambda kv: kv[1])


@dataclass
class AgentState:
    id: str
    name: str
    location: str                     # location id
    x: float
    y: float
    needs: Needs = field(default_factory=Needs)
    inventory: dict[str, int] = field(default_factory=dict)
    alive: bool = True
    cause_of_death: str | None = None
    current: Action | None = None     # action in progress
    busy_until: int = -1              # tick when current action resolves
    moving_to: str | None = None
    addressed_by: list[str] = field(default_factory=list)   # names who spoke to me since my last decision
    last_decision_tick: int = -999
    speech_count: int = 0

    def is_idle(self, tick: int) -> bool:
        return self.alive and (self.current is None or tick >= self.busy_until)

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id, "name": self.name, "location": self.location,
            "x": round(self.x, 1), "y": round(self.y, 1),
            "moving_to": self.moving_to,
            "action": self.current.type.value if self.current else None,
            "action_target": self.current.target if self.current else None,
            "alive": self.alive,
            "needs": self.needs.to_dict(),
            "inventory": dict(self.inventory),
        }


@dataclass
class World:
    clock: Clock
    locations: dict[str, Location]
    agents: dict[str, AgentState]
    weather: str = "clear"            # clear | overcast | rain | storm | cold
    weather_ticks_left: int = 0
    rng: random.Random = field(default_factory=lambda: random.Random(7))
    camp_id: str = "camp"

    # --- helpers ---
    @property
    def camp(self) -> Location:
        return self.locations[self.camp_id]

    def living(self) -> list[AgentState]:
        return [a for a in self.agents.values() if a.alive]

    def agents_at(self, location_id: str, exclude: str | None = None) -> list[AgentState]:
        return [a for a in self.agents.values()
                if a.alive and a.location == location_id and a.id != exclude and a.moving_to is None]

    def agent_by_name(self, name: str) -> AgentState | None:
        n = name.strip().lower()
        for a in self.agents.values():
            if a.name.lower() == n or a.id == n:
                return a
        return None

    def location_by_name(self, name: str) -> Location | None:
        n = name.strip().lower()
        for loc in self.locations.values():
            if loc.id == n or loc.name.lower() == n:
                return loc
        return None

    def travel_ticks(self, from_id: str, to_id: str, speed_m_per_tick: float = 400.0) -> int:
        d = self.locations[from_id].distance_to(self.locations[to_id])
        return max(1, int(math.ceil(d / speed_m_per_tick)))

    def snapshot(self) -> dict[str, Any]:
        return {
            "type": "snapshot",
            "tick": self.clock.tick,
            "time": self.clock.to_dict(),
            "weather": self.weather,
            "locations": [loc.to_dict() for loc in self.locations.values()],
            "agents": [a.to_dict() for a in self.agents.values()],
        }


def build_default_world(seed: int = 7, tick_minutes: int = 10) -> World:
    """A small valley. Distances in meters. Camp is the hub; everything else is a short walk."""
    locs = {
        "camp": Location(
            id="camp", name="Camp", x=0, y=0,
            description="A clearing with a fire pit and space for a shelter. The shared stockpile is here.",
            structures={
                "fire": Structure(kind="fire", built=True, lit=False, fuel=0),
                "shelter": Structure(kind="shelter", built=False, required=8),
            },
        ),
        "forest": Location(
            id="forest", name="Forest", x=450, y=120,
            description="Dense pines. Deadfall for firewood. Dark and cold at night.",
            resources={"wood": 40}, max_resources={"wood": 60}, regen={"wood": 0.15},
        ),
        "river": Location(
            id="river", name="River", x=-380, y=260,
            description="Fast, clear water. Fish in the pools if you're patient.",
            resources={"water": 999, "fish": 12}, max_resources={"water": 999, "fish": 16}, regen={"fish": 0.08},
        ),
        "meadow": Location(
            id="meadow", name="Meadow", x=120, y=-420,
            description="Open grass with berry bushes along the edge.",
            resources={"berries": 30}, max_resources={"berries": 40}, regen={"berries": 0.1},
        ),
        "quarry": Location(
            id="quarry", name="Quarry", x=-300, y=-350,
            description="A rocky outcrop. Loose stone for building.",
            resources={"stone": 30}, max_resources={"stone": 30}, regen={"stone": 0.0},
        ),
    }
    return World(
        clock=Clock(tick_minutes=tick_minutes),
        locations=locs,
        agents={},
        rng=random.Random(seed),
    )
