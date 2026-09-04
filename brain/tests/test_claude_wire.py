"""Prove the Claude brain builds a request the SDK accepts and parses the reply into a Decision,
without touching the real API: a local HTTP server stands in for api.anthropic.com."""

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import pytest

from hearth.agents.memory import Memory
from hearth.agents.persona import PERSONAS
from hearth.config import Config
from hearth.world.actions import ActionType
from hearth.world.state import AgentState, build_default_world

CANNED_DECISION = {
    "thought": "Fire first.",
    "say": {"to": "Jonah", "text": "Jonah, grab water. I'll get the fire going."},
    "action": {"type": "move_to", "target": "Forest", "item": None, "quantity": None},
    "plan": "wood then fire",
}


class FakeAnthropic(BaseHTTPRequestHandler):
    captured: list[dict] = []

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        FakeAnthropic.captured.append(body)
        msg = {
            "id": "msg_test", "type": "message", "role": "assistant", "model": body["model"],
            "content": [{"type": "text", "text": json.dumps(CANNED_DECISION)}],
            "stop_reason": "end_turn", "stop_sequence": None,
            "usage": {"input_tokens": 1200, "output_tokens": 80, "cache_read_input_tokens": 900, "cache_creation_input_tokens": 0},
        }
        data = json.dumps(msg).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *a):  # silence
        pass


@pytest.fixture
def fake_api(monkeypatch):
    srv = ThreadingHTTPServer(("127.0.0.1", 0), FakeAnthropic)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    monkeypatch.setenv("ANTHROPIC_API_KEY", "test-key")
    monkeypatch.setenv("ANTHROPIC_BASE_URL", f"http://127.0.0.1:{srv.server_port}")
    FakeAnthropic.captured.clear()
    yield srv
    srv.shutdown()


@pytest.mark.asyncio
async def test_claude_brain_request_shape_and_parse(fake_api):
    from hearth.agents.brain_claude import ClaudeBrain

    cfg = Config(model="claude-opus-5", effort="low")
    brain = ClaudeBrain(cfg)
    world = build_default_world()
    st = AgentState(id="mara", name="Mara", location="camp", x=0, y=0)
    world.agents["mara"] = st
    d = await brain.decide(world, st, PERSONAS[0], Memory(), "TIME: Day 1, 06:00 ...")

    assert d.action_type == ActionType.MOVE_TO and d.target == "Forest"
    assert d.say_to == "Jonah" and "water" in d.say_text
    assert d.plan == "wood then fire"

    req = FakeAnthropic.captured[0]
    assert req["model"] == "claude-opus-5"
    assert req["output_config"]["effort"] == "low"
    assert req["output_config"]["format"]["type"] == "json_schema"
    assert "thought" in req["output_config"]["format"]["schema"]["properties"]
    assert req["system"][0]["cache_control"] == {"type": "ephemeral"}
    assert "Mara" in req["system"][1]["text"]
    assert brain.usage["calls"] == 1 and brain.usage["cache_read"] == 900
