# Hearth — Project Plan

> A world (Unreal Engine 5) populated by humans, each driven by a Claude agent, who must cooperate
> to survive. They talk to each other out loud. This is the Matt Shumer "voices in the living room"
> demo, built for real.

**Owner:** Roberto (rgactr@gmail.com)
**Machine:** MacBook Air, Apple M3 Pro, macOS 26.6, Python 3.13, Node 20
**Started:** 2026-09-04
**This file is the source of truth for status.** Update the checkboxes and the Decisions Log as work lands.

---

## 0. The one-paragraph design

Two processes:

1. **Brain** (`brain/`, Python) — the authoritative simulation. Owns time, weather, resources, agent
   needs (hunger / thirst / warmth / energy / health), agent memory, and every decision. Each agent is a
   Claude call: *perceive → think → say something (maybe) → act*. Speech is spoken aloud through
   text-to-speech, one distinct voice per agent. Runs fully headless in a terminal, so the "voices in the
   other room" effect works **before** Unreal is involved.
2. **Unreal** (`unreal/`, UE5 C++) — a *renderer and body* for the same world. It connects to the brain
   over a WebSocket, spawns a character per agent, walks them between locations, shows speech bubbles,
   and reflects the world state (fire lit, shelter built, rain). Unreal never decides anything.

The split means every piece is testable on its own, Unreal can be swapped/skipped, and we can iterate on
agent behavior fast (seconds) instead of at Unreal build speed (minutes).

---

## 1. Phases and status

Legend: `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked (see notes)

### Phase 1 — Headless world + agents that talk (no Unreal)
- [x] Folder structure, plan, docs
- [x] World model: locations, resources, structures, time/weather, needs decay, death
- [x] Action system: multi-tick actions with validation and resolution
- [x] Agent personas (6 seeded characters with distinct voices)
- [x] Memory: rolling episodic memory + reflection summaries
- [x] Scripted (rule-based) brain for offline testing — no API key needed
- [x] Claude brain: cached system prompt, structured-output decisions, parallel per-tick calls
- [x] Speech propagation: agents in the same location hear each other; addressed agents get to reply
- [x] Text-to-speech: macOS `say` backend, per-agent voice, sequential queue
- [x] Event log to `logs/` (JSONL) + readable console transcript
- [x] CLI: `hearth run --brain scripted|claude --voice say|none`
- [x] Tests for world rules and the scripted run
- [ ] **First real run with Claude** — needs `ANTHROPIC_API_KEY` (Roberto)
- [ ] Tune prompts after watching a few in-game days (cooperation, not monologues)

### Phase 2 — WebSocket bridge
- [x] Protocol spec (`docs/PROTOCOL.md`)
- [x] WebSocket server broadcasting world snapshots + speech events
- [x] Inbound commands (god-mode: trigger storm, drop supplies, whisper to an agent)
- [ ] Reconnect / late-join handling verified against a real Unreal client

### Phase 3 — Unreal Engine 5 world
- [x] UE5 C++ project scaffold (uproject, module, targets, config)
- [x] `HearthBridgeSubsystem` — WebSocket client, JSON parsing, event dispatch
- [x] `AHearthAgent` — character with name + speech text, moves to location targets
- [x] `AHearthLocation` — resource node / camp marker actors spawned from the world init message
- [ ] **Install Unreal Engine 5.4+ and full Xcode** (Roberto — see `docs/SETUP.md`)
- [ ] Open project, let it compile, build a landscape map with NavMesh
- [ ] Assign a humanoid skeletal mesh (Mannequin from Third Person template is fine)
- [ ] Visual state: campfire particle when lit, shelter mesh when built, rain when raining
- [ ] Day/night via directional light driven by sim time
- [ ] In-world audio: play TTS at the speaking character's position (spatialized) instead of on the Mac

### Phase 4 — Depth
- [ ] Reflection/long-term memory that survives restarts (SQLite)
- [ ] Relationships: trust/affinity between agents affects who they help
- [ ] Crafting tree: tools → faster gathering; rope → better shelter
- [ ] Emergent threats: illness, wolves at night, dwindling resources force migration
- [ ] Observer mode: type a message into the sim as "a voice from the sky"
- [ ] Better voices (ElevenLabs / OpenAI TTS backend behind the same interface)
- [ ] Cost dashboard: tokens per agent per in-game day

---

## 2. What Roberto needs to provide

| Item | Why | Status |
|---|---|---|
| `ANTHROPIC_API_KEY` exported in the shell (or `ant auth login`) | Real agents | **needed for Phase 1 final step** |
| Unreal Engine 5.4+ via Epic Games Launcher | Phase 3 | not installed |
| Full Xcode (not just Command Line Tools) | UE5 on Mac compiles C++ through Xcode | only CLT installed |
| ~60 GB free disk | UE5 + Xcode | 69 GB free now — tight, watch it |

---

## 3. Cost expectations (Claude)

Default model is `claude-opus-5` at `effort: low` for per-tick decisions, with the system prompt
(rules + persona) cached. Rough math for 6 agents deciding about every 30 real seconds:
~700 calls/hour, ~1.5K uncached input + ~200 output tokens each → on the order of **$5–10 per hour**
of continuous running. Override with `HEARTH_MODEL=claude-sonnet-5` for ~40% of that, or run with
`--tick-seconds 10` to slow the world down. The scripted brain is free.

---

## 4. Decisions log

| Date | Decision | Why |
|---|---|---|
| 2026-09-04 | Brain in Python, Unreal as a thin client | Iterate on agent behavior in seconds; Unreal not installed yet; headless demo works day one |
| 2026-09-04 | Claude via official `anthropic` SDK, `claude-opus-5`, structured JSON decisions | SDK skill defaults; JSON schema guarantees every decision is a valid action |
| 2026-09-04 | Speech = a field on the decision, not a separate call | One call per decision keeps cost and latency down; talk and act happen together like people do |
| 2026-09-04 | Agents decide only when idle or when addressed | Prevents 6 agents chattering every tick; makes conversations turn-based naturally |
| 2026-09-04 | Same-location = hearing range | Simple, and matches how the Unreal map will be laid out (named places, not open field) |
| 2026-09-04 | macOS `say` for TTS in Phase 1 | Zero setup, distinct voices, runs offline. Swappable backend later |
| 2026-09-04 | Project name "Hearth" | The campfire is the thing they must keep alive together |

---

## 5. Open questions

- How many agents for the first Unreal build? 6 is the persona count; 4 keeps costs lower.
- Should the world be persistent across runs (agents remember yesterday)? Leaning yes, Phase 4.
- Do we want the user to be able to walk around as a 7th human and talk to them? (Great demo. Phase 4+.)

---

## 6. How to work on this (for future sessions)

1. Read this file first. Check the status boxes.
2. `cd brain && .venv/bin/python -m hearth run --brain scripted --voice none --ticks 60 --tick-seconds 0` runs the world in a second and prints a transcript. If that breaks, fix it before anything else.
3. `.venv/bin/pytest` should be green.
4. Pick the next unchecked item in the lowest incomplete phase. Update this file when it lands.
5. Architecture details live in `docs/ARCHITECTURE.md`. Wire protocol in `docs/PROTOCOL.md`.
