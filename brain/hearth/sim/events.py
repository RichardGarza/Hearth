"""Events: everything notable that happens, and a tiny async bus to fan them out."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Awaitable, Callable


class EventKind(str, Enum):
    ACTION = "action"
    SPEECH = "speech"
    WEATHER = "weather"
    FIRE_LIT = "fire_lit"
    FIRE_OUT = "fire_out"
    STRUCTURE_BUILT = "structure_built"
    DEATH = "agent_died"
    SYSTEM = "system"        # god-mode / observer messages
    THOUGHT = "thought"      # agent inner thought (never heard by others; logged)
    REFLECTION = "reflection"


@dataclass
class Event:
    kind: EventKind
    text: str
    location: str | None = None     # where it happened; None = everywhere
    agent: str | None = None        # who caused it
    to: str | None = None           # speech: addressed agent id, or None
    public: bool = False            # True = every agent perceives it regardless of location
    tick: int = 0
    extra: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        d = {"type": "event", "kind": self.kind.value, "tick": self.tick, "text": self.text,
             "agent": self.agent, "location": self.location}
        if self.kind == EventKind.SPEECH:
            d["type"] = "speech"
            d["to"] = self.to
        d.update(self.extra)
        return d


Subscriber = Callable[[Event], Awaitable[None] | None]


class EventBus:
    def __init__(self) -> None:
        self._subs: list[Subscriber] = []

    def subscribe(self, fn: Subscriber) -> None:
        self._subs.append(fn)

    async def publish(self, event: Event) -> None:
        for fn in self._subs:
            r = fn(event)
            if asyncio.iscoroutine(r):
                await r

    async def publish_all(self, events: list[Event]) -> None:
        for e in events:
            await self.publish(e)
