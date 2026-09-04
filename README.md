# Hearth

A living world of Claude-powered people who have to work together to survive — and who talk to each
other out loud while they do it.

```
Hearth/
├── PLAN.md            ← project plan and status. Start here.
├── docs/              ← architecture, wire protocol, setup
├── brain/             ← Python simulation + agents + voices (runs on its own)
├── unreal/Hearth/     ← Unreal Engine 5 project (renders the brain's world)
└── logs/              ← transcripts and event logs from runs
```

## Quick start (no Unreal, no API key)

```bash
cd brain
.venv/bin/python -m hearth run --brain scripted --voice say --ticks 120
```

You'll hear the six of them start talking.

## Free local AI on one character (Ollama)

```bash
cd brain
.venv/bin/python -m hearth run --brain ollama --ai-agents jonah --voice say --tick-seconds 6
```
Jonah thinks and talks with a local model (qwen2.5:7b via Ollama, no API key, nothing leaves the Mac).
Everyone else is scripted. In Unreal he's marked `[ AI ]` with a cyan glow: fly up to him, press
**SPACE**, type, **ESC** to walk away. Needs `ollama serve` running (installed via Homebrew).

## Real agents

```bash
export ANTHROPIC_API_KEY=sk-ant-...
cd brain
.venv/bin/python -m hearth run --brain claude --voice say
```

Leave it running, go to another room.

See `docs/SETUP.md` for the Unreal side.
