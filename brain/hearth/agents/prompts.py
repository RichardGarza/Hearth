"""Prompts. The text lives in brain/prompts/*.md so it can be edited without touching code.

Structure of the system prompt: [WORLD_RULES (shared, cached)] + [persona sheet (per agent, cached)]
"""

from __future__ import annotations

from pathlib import Path

from hearth.agents.persona import Persona
from hearth.world.actions import ACTION_DESCRIPTIONS

PROMPT_DIR = Path(__file__).resolve().parents[2] / "prompts"


def _load(name: str) -> str:
    return (PROMPT_DIR / name).read_text(encoding="utf-8").strip()


_ACTIONS = "\n".join(f"- {a.value}: {d}" for a, d in ACTION_DESCRIPTIONS.items())
_world = _load("world_rules.md")
WORLD_RULES = _world.replace("{ACTIONS}", _ACTIONS) if "{ACTIONS}" in _world else \
    _world + "\n\nACTIONS YOU CAN TAKE (exactly one per decision)\n" + _ACTIONS
CONVERSE_RULES = _load("converse_rules.md")
REFLECTION_PROMPT = _load("reflection.md")
LOCAL_MODEL_HINT = _load("local_model_hint.md")


def system_blocks(persona: Persona) -> list[dict]:
    """Two cached blocks: shared rules (same for everyone) then this persona."""
    return [
        {"type": "text", "text": WORLD_RULES, "cache_control": {"type": "ephemeral"}},
        {"type": "text", "text": "WHO YOU ARE\n" + persona.sheet(), "cache_control": {"type": "ephemeral"}},
    ]
