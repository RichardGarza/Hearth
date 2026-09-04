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
  "meters_to_units": 10,
  "tick_seconds": 3.0,
  "travel_meters_per_tick": 400
}
```
`meters_to_units` scales sim meters to Unreal units (the sim valley is ~1 km across; at 10 the map is
~100 m). `tick_seconds` and `travel_meters_per_tick` let the client pick a walk speed so trips take the
same wall-clock time on screen as in the sim.

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
`x`, `y` are sim meters. Unreal multiplies by `meters_to_units`. `moving_to` is the location id the
agent is walking toward, or null.

### `speech`
```json
{"type": "speech", "tick": 42, "agent": "mara", "to": "jonah", "text": "Jonah, bring water back if you can.", "location": "camp"}
```

### `reply` (answer to a `talk`)
```json
{"type": "reply", "tick": 42, "agent": "jonah", "text": "We don't have a leader yet.", "visitor_text": "Who's in charge?"}
```

Agent objects in `world_init` and `snapshot` also carry `"ai": true|false` (driven by a real model,
can be talked to) and `"talking": true|false`.

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

### `talk` / `talk_end` (typed dialogue with an AI character)
```json
{"type": "talk", "agent": "jonah", "text": "Hey, what's going on here?"}
{"type": "talk_end", "agent": "jonah"}
```
While a conversation is open the character stops and waits. If the visitor is silent for 2 minutes the
brain ends it. Each `talk` produces a `reply` frame (below) and a normal `speech` frame (so it's spoken
aloud and heard by anyone standing there).

### `visitor_gather` / `visitor_deposit` (the player picking things up)
```json
{"type": "visitor_gather", "location": "river"}
{"type": "visitor_deposit"}
```
Unreal owns the timing (stand at a place ~10 s → `visitor_gather`; arrive at camp carrying something →
`visitor_deposit`). The brain owns the inventory: it takes one unit of the place's main resource
(forest wood, river fish, meadow berries, quarry stone) if any is left, and moves everything into the
camp stockpile on deposit. People nearby see it happen. The brain answers with:

### `visitor_state`
```json
{"type": "visitor_state", "inventory": {"fish": 2}, "last": "+1 fish (carrying 2)"}
```
Also sent once on connect.

### `arrived` (optional; Unreal tells the brain a character physically reached its target)
```json
{"type": "arrived", "agent": "mara", "location": "camp"}
```
Not required — the brain simulates travel time itself. This exists so Unreal can later become the
authority on movement.
