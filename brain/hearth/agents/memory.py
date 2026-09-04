"""Per-agent memory: a rolling window of witnessed events plus periodic reflections."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field


@dataclass
class Memory:
    window: int = 30
    reflect_every: int = 40
    episodic: deque[str] = field(default_factory=deque)
    reflections: list[str] = field(default_factory=list)   # compressed long-term notes
    plan: str = ""                                          # note-to-self from last decision
    since_reflection: int = 0
    last_feedback: str | None = None                        # e.g. "That action wasn't possible: ..."

    def __post_init__(self) -> None:
        self.episodic = deque(self.episodic, maxlen=self.window)

    def remember(self, line: str) -> None:
        self.episodic.append(line)
        self.since_reflection += 1

    def needs_reflection(self) -> bool:
        return self.since_reflection >= self.reflect_every

    def add_reflection(self, text: str) -> None:
        self.reflections.append(text)
        if len(self.reflections) > 5:
            self.reflections = self.reflections[-5:]
        self.since_reflection = 0

    def recent_text(self, n: int | None = None) -> str:
        items = list(self.episodic)[-(n or self.window):]
        return "\n".join(f"- {x}" for x in items) if items else "- (nothing yet)"

    def reflections_text(self) -> str:
        return "\n".join(f"- {r}" for r in self.reflections) if self.reflections else "- (none yet)"
