"""Characters. Defined in brain/prompts/personas.toml so they can be edited without code."""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path

PERSONA_FILE = Path(__file__).resolve().parents[2] / "prompts" / "personas.toml"


@dataclass(frozen=True)
class Persona:
    id: str
    name: str
    age: int
    voice: str                    # macOS `say` voice
    background: str
    traits: tuple[str, ...]
    speaking_style: str
    private_worry: str            # something they carry that colors decisions
    body: str = "manny"           # Unreal mannequin: "manny" (male) or "quinn" (female)
    start_location: str = "camp"
    start_inventory: dict[str, int] = field(default_factory=dict)

    def sheet(self) -> str:
        return (
            f"Name: {self.name}, age {self.age}.\n"
            f"Background: {self.background}\n"
            f"Traits: {', '.join(self.traits)}.\n"
            f"How you talk: {self.speaking_style}\n"
            f"What you privately worry about: {self.private_worry}"
        )


def load_personas(path: Path = PERSONA_FILE) -> list[Persona]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    out = []
    for d in data.get("persona", []):
        out.append(Persona(
            id=str(d["id"]).lower(), name=d["name"], age=int(d["age"]), voice=d.get("voice", "Samantha"),
            background=d.get("background", ""), traits=tuple(d.get("traits", [])),
            speaking_style=d.get("speaking_style", ""), private_worry=d.get("private_worry", ""),
            body=d.get("body", "manny"), start_location=d.get("start_location", "camp"),
            start_inventory=dict(d.get("start_inventory", {})),
        ))
    return out


PERSONAS: list[Persona] = load_personas()


def personas(n: int | None = None) -> list[Persona]:
    return PERSONAS[: n or len(PERSONAS)]
