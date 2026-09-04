# Architecture

## Processes

```
┌──────────────────────────────────────────────┐        ┌──────────────────────────┐
│ brain  (Python, asyncio)                     │  ws    │ unreal  (UE5, C++)       │
│                                              │◄──────►│                          │
│  Engine tick loop                            │ :8765  │ HearthBridgeSubsystem    │
│   ├─ World: time, weather, resources         │        │   ├─ spawns AHearthAgent │
│   ├─ Needs decay, action resolution          │        │   ├─ spawns AHearthLoc   │
│   ├─ Perception per idle agent               │        │   └─ speech bubbles      │
│   ├─ Brain.decide()  ──► Claude API          │        └──────────────────────────┘
│   ├─ Apply decisions (actions + speech)       │
│   └─ Event bus ──► log, TTS, websocket        │
└──────────────────────────────────────────────┘
```

## Brain package layout

| Module | Responsibility |
|---|---|
| `hearth/config.py` | Env-driven settings (model, effort, tick pacing, ports) |
| `hearth/world/state.py` | Dataclasses: `World`, `Location`, `AgentState`, `Structure`, resource stocks |
| `hearth/world/rules.py` | Needs decay, weather, regen, death, what counts as "night" / "cold" |
| `hearth/world/actions.py` | `ActionType` enum, validation, duration, and resolution effects |
| `hearth/agents/persona.py` | Seed characters: name, backstory, traits, macOS voice |
| `hearth/agents/memory.py` | Episodic memory ring + reflection summaries |
| `hearth/agents/perception.py` | Builds what an agent can see/hear/feel this tick |
| `hearth/agents/schema.py` | `Decision` JSON schema (shared by Claude and scripted brains) |
| `hearth/agents/brain_claude.py` | Claude implementation (structured output, cached system prompt) |
| `hearth/agents/brain_scripted.py` | Rule-based fallback for offline runs and tests |
| `hearth/agents/prompts.py` | System prompt text |
| `hearth/sim/events.py` | Event types and the async event bus |
| `hearth/sim/engine.py` | The tick loop |
| `hearth/voice/tts.py` | TTS backends (`say`, null) and the sequential speech queue |
| `hearth/server/ws.py` | WebSocket broadcaster + inbound commands |
| `hearth/cli.py` | `python -m hearth run ...` |

## The tick

One tick = 10 minutes of world time. Default real-time pacing is 3 seconds per tick, so a world day
is about 7 real minutes.

1. Advance clock; roll weather.
2. Regenerate resources (slowly, capped).
3. Decay needs for every living agent (rate depends on activity, weather, fire, shelter).
4. Resolve actions that finish this tick → produce events + memories.
5. Check death.
6. For each agent that is idle **or was addressed by name last tick**, build perception and ask its brain.
   All brain calls run concurrently.
7. Apply decisions: enqueue actions, emit `speech` events (heard by everyone at the same location,
   spoken by TTS, sent to Unreal).
8. Broadcast snapshot.

## Why decisions are one call

Each decision returns `{thought, say, action, plan}` in one structured response. Splitting "what do I
say" from "what do I do" doubles cost and makes agents say things unrelated to what they then do.

## Memory

- **Episodic**: last ~30 events the agent witnessed, as short sentences with timestamps.
- **Reflection**: every ~40 events, the brain is asked to compress older memories into 3–5 lines of
  "what I know / what I've agreed to / how I feel about people". Stored on the agent.
- **Plan**: a free-text note the agent writes to itself each decision; shown back next time.

## Determinism

The world uses a seeded RNG so the scripted brain produces repeatable runs for tests. Claude runs are
not deterministic and that's the point.
