"""Connect to a running brain, print the first few frames, send a god-mode command.

    .venv/bin/python scripts/ws_probe.py [ws://127.0.0.1:8765]
"""

import asyncio
import json
import sys

from websockets.asyncio.client import connect


async def main(url: str) -> None:
    async with connect(url) as ws:
        for i in range(6):
            msg = json.loads(await ws.recv())
            t = msg["type"]
            if t == "world_init":
                print("world_init:", [l["id"] for l in msg["locations"]], [a["name"] for a in msg["agents"]])
            elif t == "snapshot":
                print(f"snapshot tick={msg['tick']} {msg['time']} weather={msg['weather']} "
                      f"agents={[(a['id'], a['location'], a['action']) for a in msg['agents']][:3]}...")
            else:
                print(t, {k: v for k, v in msg.items() if k != 'type'})
            if i == 2:
                await ws.send(json.dumps({"type": "command", "name": "drop_supplies", "location": "camp", "items": {"fish": 5}}))


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1] if len(sys.argv) > 1 else "ws://127.0.0.1:8765"))
