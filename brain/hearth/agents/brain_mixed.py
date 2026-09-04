"""Route each agent to its own brain: e.g. one person on a real model, everyone else scripted."""

from __future__ import annotations

from hearth.agents.brain_base import Brain
from hearth.agents.memory import Memory
from hearth.agents.persona import Persona
from hearth.agents.schema import Decision
from hearth.world.state import AgentState, World


class MixedBrain:
    def __init__(self, default: Brain, overrides: dict[str, Brain]) -> None:
        self.default = default
        self.overrides = overrides

    def for_agent(self, agent_id: str) -> Brain:
        return self.overrides.get(agent_id, self.default)

    def is_ai(self, agent_id: str) -> bool:
        return agent_id in self.overrides

    async def decide(self, world: World, agent: AgentState, persona: Persona, memory: Memory, perception: str) -> Decision:
        return await self.for_agent(agent.id).decide(world, agent, persona, memory, perception)

    async def reflect(self, persona: Persona, memory: Memory) -> str | None:
        return await self.for_agent(persona.id).reflect(persona, memory)

    async def converse(self, world: World, agent: AgentState, persona: Persona, memory: Memory,
                       history: list[tuple[str, str]], visitor_text: str) -> str:
        brain = self.for_agent(agent.id)
        if hasattr(brain, "converse"):
            return await brain.converse(world, agent, persona, memory, history, visitor_text)
        return "..."

    def estimated_cost(self) -> float:
        return sum(getattr(b, "estimated_cost", lambda: 0.0)() for b in {id(b): b for b in self.overrides.values()}.values())

    def usage_line(self) -> str:
        parts = [f"{aid}: {b.usage_line()}" for aid, b in self.overrides.items() if hasattr(b, "usage_line")]
        return "; ".join(parts) or "scripted only"
