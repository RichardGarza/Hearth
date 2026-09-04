"""Claude-backed brain.

One call per decision. The system prompt (world rules + persona) is cached; the perception goes in
the user turn. The response is constrained to DECISION_SCHEMA via structured output, so every
decision parses into a valid action type.
"""

from __future__ import annotations

import asyncio
import json
import logging

import anthropic

from hearth.agents.memory import Memory
from hearth.agents.persona import Persona
from hearth.agents.prompts import REFLECTION_PROMPT, system_blocks
from hearth.agents.schema import DECISION_SCHEMA, Decision
from hearth.config import Config
from hearth.world.state import AgentState, World

log = logging.getLogger("hearth.claude")


class ClaudeBrain:
    def __init__(self, cfg: Config) -> None:
        self.cfg = cfg
        self.client = anthropic.AsyncAnthropic(max_retries=3, timeout=90.0)
        self.sem = asyncio.Semaphore(cfg.max_concurrent_calls)
        self.usage = {"calls": 0, "input": 0, "output": 0, "cache_read": 0, "cache_write": 0, "refusals": 0, "errors": 0}

    # ------------------------------------------------------------------ decide
    async def decide(self, world: World, agent: AgentState, persona: Persona, memory: Memory, perception: str) -> Decision:
        kwargs = dict(
            model=self.cfg.model,
            max_tokens=self.cfg.max_decision_tokens,
            system=system_blocks(persona),
            messages=[{"role": "user", "content": perception}],
            output_config={"effort": self.cfg.effort, "format": {"type": "json_schema", "schema": DECISION_SCHEMA}},
        )
        async with self.sem:
            try:
                if self.cfg.fallbacks:
                    resp = await self.client.beta.messages.create(
                        betas=["server-side-fallback-2026-07-01"], fallbacks="default", **kwargs)
                else:
                    resp = await self.client.messages.create(**kwargs)
            except anthropic.RateLimitError as e:
                self.usage["errors"] += 1
                log.warning("%s: rate limited (%s); waiting this turn out", persona.name, e.message)
                return Decision.wait(thought="(rate limited)", plan=memory.plan)
            except anthropic.APIStatusError as e:
                self.usage["errors"] += 1
                log.error("%s: API error %s: %s", persona.name, e.status_code, e.message)
                return Decision.wait(thought=f"(api error {e.status_code})", plan=memory.plan)
            except anthropic.APIConnectionError as e:
                self.usage["errors"] += 1
                log.error("%s: connection error: %s", persona.name, e)
                return Decision.wait(thought="(connection error)", plan=memory.plan)

        self._track(resp)
        if resp.stop_reason == "refusal":
            self.usage["refusals"] += 1
            log.warning("%s: refusal (%s)", persona.name, getattr(resp.stop_details, "category", None))
            return Decision.wait(thought="(no decision)", plan=memory.plan)
        if resp.stop_reason == "max_tokens":
            log.warning("%s: hit max_tokens; treating as wait", persona.name)
            return Decision.wait(thought="(cut off)", plan=memory.plan)

        text = next((b.text for b in resp.content if b.type == "text"), "")
        try:
            data = json.loads(text)
        except json.JSONDecodeError:
            log.error("%s: unparseable decision: %r", persona.name, text[:200])
            return Decision.wait(thought="(bad json)", plan=memory.plan)
        return Decision.from_json(data)

    # ------------------------------------------------------------------ reflect
    async def reflect(self, persona: Persona, memory: Memory) -> str | None:
        content = REFLECTION_PROMPT + "\n\nMEMORIES:\n" + memory.recent_text() + \
            "\n\nPREVIOUS NOTES:\n" + memory.reflections_text()
        async with self.sem:
            try:
                resp = await self.client.messages.create(
                    model=self.cfg.model,
                    max_tokens=1000,
                    system=system_blocks(persona),
                    messages=[{"role": "user", "content": content}],
                    output_config={"effort": "low"},
                )
            except anthropic.APIError as e:
                log.error("%s: reflection failed: %s", persona.name, e)
                return None
        self._track(resp)
        if resp.stop_reason == "refusal":
            return None
        return next((b.text for b in resp.content if b.type == "text"), None)

    # ------------------------------------------------------------------ usage
    def _track(self, resp) -> None:
        u = resp.usage
        self.usage["calls"] += 1
        self.usage["input"] += u.input_tokens
        self.usage["output"] += u.output_tokens
        self.usage["cache_read"] += u.cache_read_input_tokens or 0
        self.usage["cache_write"] += u.cache_creation_input_tokens or 0

    def usage_line(self) -> str:
        u = self.usage
        return (f"calls={u['calls']} in={u['input']} cache_read={u['cache_read']} cache_write={u['cache_write']} "
                f"out={u['output']} refusals={u['refusals']} errors={u['errors']}")
