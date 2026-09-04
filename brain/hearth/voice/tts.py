"""Text-to-speech. One sequential queue so voices don't talk over each other.

Backends implement `speak(text, voice)`. `SayBackend` uses macOS `say`. Add ElevenLabs/OpenAI
later behind the same interface (see PLAN.md Phase 4).
"""

from __future__ import annotations

import asyncio
import logging
import shutil
import subprocess
from typing import Protocol

from hearth.sim.events import Event, EventKind

log = logging.getLogger("hearth.voice")


class TTSBackend(Protocol):
    async def speak(self, text: str, voice: str) -> None: ...


class NullBackend:
    async def speak(self, text: str, voice: str) -> None:
        return None


class SayBackend:
    def __init__(self, rate: int = 180) -> None:
        self.rate = rate
        self.available = self._list_voices()
        if not shutil.which("say"):
            log.warning("`say` not found; voices disabled")

    @staticmethod
    def _list_voices() -> set[str]:
        try:
            out = subprocess.run(["say", "-v", "?"], capture_output=True, text=True, timeout=10).stdout
        except Exception:
            return set()
        voices = set()
        for line in out.splitlines():
            # "Samantha            en_US    # Hello..."  — name may contain spaces before the locale column
            name = line.split("  ")[0].strip()
            if name:
                voices.add(name)
        return voices

    async def speak(self, text: str, voice: str) -> None:
        args = ["say", "-r", str(self.rate)]
        if voice in self.available:
            args += ["-v", voice]
        args.append(text)
        try:
            proc = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.DEVNULL,
                                                        stderr=asyncio.subprocess.DEVNULL)
            await proc.wait()
        except FileNotFoundError:
            pass


class SpeechQueue:
    """Subscribes to the event bus; speaks SPEECH events in order."""

    def __init__(self, backend: TTSBackend) -> None:
        self.backend = backend
        self.q: asyncio.Queue[tuple[str, str]] = asyncio.Queue()
        self._task: asyncio.Task | None = None

    def start(self) -> None:
        self._task = asyncio.create_task(self._worker())

    async def stop(self) -> None:
        if self._task:
            self._task.cancel()

    def on_event(self, e: Event) -> None:
        if e.kind == EventKind.SPEECH:
            self.q.put_nowait((e.extra.get("voice", ""), e.text))

    async def drain(self) -> None:
        await self.q.join()

    async def _worker(self) -> None:
        while True:
            voice, text = await self.q.get()
            try:
                await self.backend.speak(text, voice)
            except Exception:
                log.exception("tts failed")
            finally:
                self.q.task_done()
