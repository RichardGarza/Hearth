"""Talk to an AI character over the bridge, like Unreal does.
    .venv/bin/python scripts/talk_probe.py jonah "Hey, what's going on?" [ws://127.0.0.1:8765]
"""
import asyncio, json, sys
from websockets.asyncio.client import connect

async def main(agent, text, url):
    async with connect(url) as ws:
        await ws.send(json.dumps({"type": "talk", "agent": agent, "text": text}))
        while True:
            msg = json.loads(await asyncio.wait_for(ws.recv(), 120))
            if msg.get("type") == "reply" and msg.get("agent") == agent:
                print(f"{agent}: {msg['text']}")
                break
        await ws.send(json.dumps({"type": "talk_end", "agent": agent}))

if __name__ == "__main__":
    a = sys.argv[1:] + [None] * 3
    asyncio.run(main(a[0] or "jonah", a[1] or "Hey, what's going on here?", a[2] or "ws://127.0.0.1:8765"))
