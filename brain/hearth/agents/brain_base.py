"""Brain interface. A brain turns a perception into a Decision. Nothing else."""

from __future__ import annotations

from typing import Protocol

from hearth.agents.memory import Memory
from hearth.agents.persona import Persona
from hearth.agents.schema import Decision
from hearth.world.state import AgentState, World


class Brain(Protocol):
    async def decide(self, world: World, agent: AgentState, persona: Persona, memory: Memory, perception: str) -> Decision: ...

    async def reflect(self, persona: Persona, memory: Memory) -> str | None:
        """Return compressed long-term notes, or None if this brain doesn't reflect."""
        ...
