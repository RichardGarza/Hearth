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

## Play (Unreal + local AI on one character)

```bash
./play.sh
```
Starts the brain (Jonah on a local model via Ollama, everyone else scripted, voices on) and opens the
game. You walk as a third-person character: **WASD** + mouse, **Shift** to run. Jonah is marked
`[ AI ]` with a cyan glow; walk up to him, press **SPACE**, type, **ESC** to walk away. **ESC**
anywhere else opens the menu; **Quit** closes the game *and* the brain, so the voices stop.

`./play.sh --brain scripted` runs with no AI at all.

## Real agents

```bash
export ANTHROPIC_API_KEY=sk-ant-...
cd brain
.venv/bin/python -m hearth run --brain claude --voice say
```

Leave it running, go to another room.

See `docs/SETUP.md` for the Unreal side.
