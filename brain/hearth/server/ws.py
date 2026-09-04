"""WebSocket server: pushes world_init/snapshot/speech/event frames to Unreal (or any viewer),
accepts god-mode commands back. See docs/PROTOCOL.md."""

from __future__ import annotations

import asyncio
import json
import logging
from typing import Any

from websockets.asyncio.server import Server, ServerConnection, broadcast, serve

from hearth.sim.engine import Engine
from hearth.sim.events import Event, EventKind

log = logging.getLogger("hearth.ws")


class BridgeServer:
    def __init__(self, engine: Engine, host: str, port: int) -> None:
        self.engine = engine
        self.host, self.port = host, port
        self.clients: set[ServerConnection] = set()
        self._server: Server | None = None

    async def start(self) -> None:
        self._server = await serve(self._handler, self.host, self.port)
        self.engine.bus.subscribe(self.on_event)
        self.engine.snapshot_sinks.append(self.on_snapshot)
        log.info("bridge listening on ws://%s:%d", self.host, self.port)

    async def stop(self) -> None:
        if self._server:
            self._server.close()
            await self._server.wait_closed()

    # ---- outbound
    def _send_all(self, msg: dict[str, Any]) -> None:
        if self.clients:
            broadcast(self.clients, json.dumps(msg))

    def on_event(self, e: Event) -> None:
        if e.kind in (EventKind.THOUGHT, EventKind.REFLECTION):
            return
        self._send_all(e.to_dict())

    def on_snapshot(self, snap: dict[str, Any]) -> None:
        self._send_all(snap)

    # ---- inbound
    async def _handler(self, ws: ServerConnection) -> None:
        self.clients.add(ws)
        log.info("client connected (%d total)", len(self.clients))
        try:
            await ws.send(json.dumps(self.engine.world_init_message()))
            await ws.send(json.dumps(self.engine.world.snapshot()))
            async for raw in ws:
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    continue
                if msg.get("type") == "command":
                    kw = {k: v for k, v in msg.items() if k not in ("type", "name")}
                    result = await self.engine.command(msg.get("name", ""), **kw)
                    await ws.send(json.dumps({"type": "command_result", "name": msg.get("name"), "result": result}))
                elif msg.get("type") == "arrived":
                    pass  # brain owns movement for now; see PROTOCOL.md
        except Exception as e:  # connection closed etc.
            log.debug("client dropped: %s", e)
        finally:
            self.clients.discard(ws)
