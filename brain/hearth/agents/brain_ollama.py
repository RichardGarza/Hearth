"""Local brain via Ollama (https://ollama.com). Free, offline, and noticeably dumber than Claude.

Uses Ollama's structured-output support (`format` = JSON schema) so decisions still parse into valid
actions. Requests are made with the standard library so no extra dependency is needed.

    ollama pull qwen2.5:7b        # ~4.7 GB; follows JSON schemas well
    hearth run --brain ollama
"""

from __future__ import annotations

import asyncio
import json
import logging
import urllib.error
import urllib.request

from hearth.agents.memory import Memory
from hearth.agents.persona import Persona
from hearth.agents.prompts import REFLECTION_PROMPT, WORLD_RULES
from hearth.agents.schema import DECISION_SCHEMA, Decision
from hearth.config import Config
from hearth.world.state import AgentState, World

log = logging.getLogger("hearth.ollama")

# Small models need the reminder; Claude doesn't.
LOCAL_HINT = (
    "\n\nReply with ONLY a JSON object with keys thought, say, action, plan. "
    "`say` is null or {\"to\": name-or-null, \"text\": \"...\"}. "
    "`action` is {\"type\": one of the action names, \"target\": ..., \"item\": ..., \"quantity\": ...}. "
    "Keep `say.text` under 30 words."
)


class OllamaBrain:
    def __init__(self, cfg: Config) -> None:
        self.cfg = cfg
        self.url = cfg.ollama_url.rstrip("/")
        self.model = cfg.ollama_model
        self.sem = asyncio.Semaphore(cfg.ollama_parallel)
        self.usage = {"calls": 0, "errors": 0, "prompt_tokens": 0, "output_tokens": 0, "seconds": 0.0}

    # ------------------------------------------------------------------ http
    def _post(self, payload: dict, timeout: float) -> dict:
        req = urllib.request.Request(
            f"{self.url}/api/chat", data=json.dumps(payload).encode(),
            headers={"Content-Type": "application/json"}, method="POST")
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read())

    async def _chat(self, system: str, user: str, schema: dict | None, max_tokens: int) -> str | None:
        payload = {
            "model": self.model,
            "stream": False,
            "messages": [{"role": "system", "content": system}, {"role": "user", "content": user}],
            "options": {"temperature": 0.8, "num_predict": max_tokens, "num_ctx": self.cfg.ollama_ctx},
        }
        if schema is not None:
            payload["format"] = schema
        async with self.sem:
            try:
                data = await asyncio.to_thread(self._post, payload, self.cfg.ollama_timeout)
            except urllib.error.URLError as e:
                self.usage["errors"] += 1
                log.error("ollama unreachable at %s (%s). Is `ollama serve` running and the model pulled?", self.url, e.reason)
                return None
            except (TimeoutError, OSError) as e:
                self.usage["errors"] += 1
                log.error("ollama request failed: %s", e)
                return None
        self.usage["calls"] += 1
        self.usage["prompt_tokens"] += int(data.get("prompt_eval_count", 0))
        self.usage["output_tokens"] += int(data.get("eval_count", 0))
        self.usage["seconds"] += float(data.get("total_duration", 0)) / 1e9
        if "error" in data:
            self.usage["errors"] += 1
            log.error("ollama error: %s", data["error"])
            return None
        return (data.get("message") or {}).get("content")

    # ------------------------------------------------------------------ brain
    async def decide(self, world: World, agent: AgentState, persona: Persona, memory: Memory, perception: str) -> Decision:
        system = WORLD_RULES + "\n\nWHO YOU ARE\n" + persona.sheet()
        text = await self._chat(system, perception + LOCAL_HINT, DECISION_SCHEMA, self.cfg.max_decision_tokens)
        if not text:
            return Decision.wait(thought="(no answer from local model)", plan=memory.plan)
        try:
            return Decision.from_json(json.loads(text))
        except (json.JSONDecodeError, AttributeError, TypeError):
            log.error("%s: unparseable local decision: %r", persona.name, text[:200])
            return Decision.wait(thought="(bad json)", plan=memory.plan)

    async def reflect(self, persona: Persona, memory: Memory) -> str | None:
        system = WORLD_RULES + "\n\nWHO YOU ARE\n" + persona.sheet()
        user = REFLECTION_PROMPT + "\n\nMEMORIES:\n" + memory.recent_text() + "\n\nPREVIOUS NOTES:\n" + memory.reflections_text()
        return await self._chat(system, user, None, 400)

    def usage_line(self) -> str:
        u = self.usage
        avg = u["seconds"] / u["calls"] if u["calls"] else 0
        return (f"model={self.model} calls={u['calls']} prompt_tokens={u['prompt_tokens']} out={u['output_tokens']} "
                f"avg={avg:.1f}s/call errors={u['errors']} cost=$0")
