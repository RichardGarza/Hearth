"""Runtime configuration, read from environment variables with sane defaults."""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path


def _env(name: str, default: str) -> str:
    return os.environ.get(name, default)


@dataclass
class Config:
    # --- Claude ---
    model: str = field(default_factory=lambda: _env("HEARTH_MODEL", "claude-opus-5"))
    effort: str = field(default_factory=lambda: _env("HEARTH_EFFORT", "low"))
    fallbacks: bool = field(default_factory=lambda: _env("HEARTH_FALLBACKS", "0") == "1")
    max_decision_tokens: int = 2000
    max_concurrent_calls: int = 6
    budget_usd: float | None = None  # stop the run when estimated Claude spend reaches this

    # --- Ollama (local, free) ---
    ollama_url: str = field(default_factory=lambda: _env("HEARTH_OLLAMA_URL", "http://127.0.0.1:11434"))
    ollama_model: str = field(default_factory=lambda: _env("HEARTH_OLLAMA_MODEL", "qwen2.5:7b"))
    ollama_parallel: int = 2
    ollama_ctx: int = 8192
    ollama_timeout: float = 120.0

    # --- Simulation pacing ---
    tick_minutes: int = 10           # world minutes per tick
    tick_seconds: float = 3.0        # real seconds per tick (0 = as fast as possible)
    max_ticks: int | None = None     # None = run forever

    # --- Memory ---
    episodic_window: int = 30
    reflect_every: int = 40

    # --- Voice ---
    voice_backend: str = "say"       # "say" | "none"
    speech_rate: int = 180           # words per minute for `say`

    # --- Server ---
    ws_host: str = "127.0.0.1"
    ws_port: int = field(default_factory=lambda: int(_env("HEARTH_WS_PORT", "8765")))
    ws_enabled: bool = True

    # --- Logging ---
    log_dir: Path = field(default_factory=lambda: Path(__file__).resolve().parents[2] / "logs")

    # --- Determinism ---
    seed: int = 7
