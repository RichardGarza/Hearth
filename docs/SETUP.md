# Setup

## Brain (already done on this Mac)

```bash
cd brain
python3 -m venv .venv
.venv/bin/pip install -e .
.venv/bin/pytest
```

Real agents need a key:

```bash
export ANTHROPIC_API_KEY=sk-ant-...
```

Optional env vars (see `brain/hearth/config.py`):

| Var | Default | Meaning |
|---|---|---|
| `HEARTH_MODEL` | `claude-opus-5` | Model for agent decisions |
| `HEARTH_EFFORT` | `low` | `low`/`medium`/`high` — decision depth vs. cost |
| `HEARTH_FALLBACKS` | `0` | `1` enables Anthropic server-side refusal fallbacks (beta) |
| `HEARTH_WS_PORT` | `8765` | WebSocket port for Unreal |

## Unreal Engine (to do)

1. Install **Xcode** from the App Store (full Xcode; UE5 needs it to compile C++ on macOS).
   Then run once: `sudo xcode-select -s /Applications/Xcode.app` and open Xcode to accept the license.
2. Install the **Epic Games Launcher**, sign in, install **Unreal Engine 5.4** or newer.
3. Open `unreal/Hearth/Hearth.uproject`. When asked to rebuild the module, say yes.
   First compile takes 10–20 minutes on an M3.
4. In the editor: create a level (Landscape or just a big plane), add a **NavMeshBoundsVolume** covering
   it, press `P` to confirm green nav coverage, set it as the default map in Project Settings → Maps.
5. Project Settings → Maps & Modes → GameMode = `HearthGameMode`.
6. Press Play. The bridge connects to `ws://127.0.0.1:8765`; start the brain first:
   ```bash
   cd brain && .venv/bin/python -m hearth run --brain claude --voice say
   ```
7. Characters appear as capsules with name labels until you assign a skeletal mesh in
   `AHearthAgent` (Blueprint subclass recommended: `BP_HearthAgent`, set it in `HearthGameMode`).
