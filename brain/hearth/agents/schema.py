"""The Decision every brain must return, and its JSON schema for Claude structured output."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from hearth.world.actions import ActionType

DECISION_SCHEMA: dict[str, Any] = {
    "type": "object",
    "properties": {
        "thought": {
            "type": "string",
            "description": "One or two sentences of private reasoning. Nobody hears this.",
        },
        "say": {
            "type": ["object", "null"],
            "description": "Something you say out loud right now, or null to stay quiet. Only people at your location hear it.",
            "properties": {
                "to": {"type": ["string", "null"], "description": "Name of the person you're addressing, or null for everyone present."},
                "text": {"type": "string", "description": "What you say. Natural spoken language, 1-3 sentences."},
            },
            "required": ["to", "text"],
            "additionalProperties": False,
        },
        "action": {
            "type": "object",
            "properties": {
                "type": {"type": "string", "enum": [a.value for a in ActionType]},
                "target": {"type": ["string", "null"], "description": "Location name, resource name, person name, or 'shelter' depending on the action."},
                "item": {"type": ["string", "null"], "description": "For give: the resource to hand over."},
                "quantity": {"type": ["integer", "null"], "description": "For give/take: how many."},
            },
            "required": ["type", "target", "item", "quantity"],
            "additionalProperties": False,
        },
        "plan": {
            "type": "string",
            "description": "A short note to your future self: what you intend to do next and why. Shown back to you next time.",
        },
    },
    "required": ["thought", "say", "action", "plan"],
    "additionalProperties": False,
}


@dataclass
class Decision:
    thought: str
    say_text: str | None
    say_to: str | None
    action_type: ActionType
    target: str | None = None
    item: str | None = None
    quantity: int | None = None
    plan: str = ""
    raw: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_json(cls, d: dict[str, Any]) -> "Decision":
        say = d.get("say") or None
        act = d.get("action") or {}
        try:
            at = ActionType(act.get("type", "wait"))
        except ValueError:
            at = ActionType.WAIT
        return cls(
            thought=str(d.get("thought", "")),
            say_text=(say or {}).get("text") if say else None,
            say_to=(say or {}).get("to") if say else None,
            action_type=at,
            target=act.get("target"),
            item=act.get("item"),
            quantity=act.get("quantity"),
            plan=str(d.get("plan", "")),
            raw=d,
        )

    @classmethod
    def wait(cls, thought: str = "", plan: str = "") -> "Decision":
        return cls(thought=thought, say_text=None, say_to=None, action_type=ActionType.WAIT, plan=plan)
