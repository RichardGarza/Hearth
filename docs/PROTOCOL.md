# Brain ⇄ Unreal wire protocol

Transport: WebSocket, `ws://127.0.0.1:8765`. Every frame is one JSON object with a `type` field.
Brain is the server; Unreal (or any viewer) is a client. Multiple clients allowed.

## Server → client

### `world_init` (sent once on connect)
```json
{
  "type": "world_init",
  "locations": [
    {"id": "camp", "name": "Camp", "x": 0, "y": 0,
     "resources": {"wood": 0}, "structures": {"fire": {"lit": false, "fuel": 0}}}
  ],
  "agents": [
    {"id": "mara", "name": "Mara", "voice": "Samantha", "location": "camp", "x": 0, "y": 0}
  ],
  "meters_to_units": 100
}
```

### `snapshot` (every tick)
```json
{
  "type": "snapshot",
  "tick": 42,
  "time": {"day": 1, "hour": 7, "minute": 0, "is_night": false},
  "weather": "rain",
  "locations": [{"id": "camp", "resources": {...}, "structures": {...}}],
  "agents": [
    {"id": "mara", "location": "forest", "x": 120, "y": -40, "moving_to": "camp",
     "action": "gather", "alive": true,
     "needs": {"hunger": 61, "thirst": 40, "energy": 70, "warmth": 55, "health": 100},
     "inventory": {"wood": 3}}
  ]
}
```
`x`, `y` are world meters. Unreal multiplies by `meters_to_units`. `moving_to` is the location id the
agent is walking toward, or null.

### `speech`
```json
{"type": "speech", "tick": 42, "agent": "mara", "to": "jonah", "text": "Jonah, bring water back if you can.", "location": "camp"}
```

### `event` (anything notable — for HUD/log)
```json
{"type": "event", "tick": 42, "kind": "fire_lit" | "structure_built" | "agent_died" | "storm" | ..., "text": "Mara lit the fire.", "agent": "mara", "location": "camp"}
```

## Client → server

### `command` (god mode / debugging)
```json
{"type": "command", "name": "storm"}
{"type": "command", "name": "drop_supplies", "location": "camp", "items": {"berries": 10}}
{"type": "command", "name": "whisper", "agent": "mara", "text": "You feel a chill. Winter is coming."}
{"type": "command", "name": "pause"}   /  {"type": "command", "name": "resume"}
```

### `arrived` (optional; Unreal tells the brain a character physically reached its target)
```json
{"type": "arrived", "agent": "mara", "location": "camp"}
```
Not required — the brain simulates travel time itself. This exists so Unreal can later become the
authority on movement.
