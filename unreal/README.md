# Hearth — Unreal side

`Hearth/` is a UE5 C++ project (compiles on UE 5.8.2 / Xcode 26.6). The code is a WebSocket
*viewer* for the Python brain in `../brain`.

## What's in `Source/Hearth`

| File | Role |
|---|---|
| `HearthBridgeSubsystem` | GameInstance subsystem. Connects to `ws://127.0.0.1:8765`, parses frames, keeps latest `Locations`/`Agents`, fires `OnWorldInit`/`OnSnapshot`/`OnSpeech`/`OnEvent`. Auto-reconnects. `SendCommand("storm")` etc. for god mode. |
| `HearthGameMode` | Spawns one `AHearthLocation` per place and one `AHearthAgent` per person on `world_init`; on each snapshot walks agents to their target and updates labels. Default pawn is a spectator so you can fly around. |
| `AHearthAgent` | `ACharacter` with name / status / speech text above the head, moves via `AAIController::MoveToLocation` (needs a NavMesh). Blueprint events `OnSay`, `OnActionChanged`, `OnDied` for animation and audio. |
| `AHearthLocation` | Flat cylinder marker + label listing what's here. Point light turns on when the camp fire is lit. Blueprint events `OnFireChanged`, `OnShelterChanged` for particles/meshes. |
| `HearthTypes.h` | Blueprint-visible structs mirroring `docs/PROTOCOL.md`. |

## Command-line workflow (no editor UI needed)

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
# compile the game module
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" HearthEditor Mac Development -Project="$PWD/Hearth/Hearth.uproject" -WaitMutex
# (re)generate the Valley map: ground, sun, sky, fog, nav bounds, player start, game mode
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Hearth/Hearth.uproject" -run=pythonscript -script="$PWD/Hearth/Scripts/build_valley_map.py" -unattended -nop4
# smoke test with the brain, no window
./run_headless_test.sh
```

## Watching it for real

1. Start the brain: `cd ../brain && .venv/bin/python -m hearth run --brain claude --voice say --tick-seconds 8`
   (slower ticks give characters time to walk at a believable speed).
2. Open `Hearth/Hearth.uproject` (double-click, or `open`). The Valley map loads. Press Play.
   You spawn as a spectator above camp: WASD + mouse to fly. Output Log filter `LogHearth`.

## Scale

The brain's world is ~1 km across in sim meters. `world_init` sends `meters_to_units`; the brain
sends 10 by default for Unreal, which makes the map ~100 m across. Characters' walk speed is derived
from the brain's travel speed and `tick_seconds` so trips take the same time on screen as in the sim.

## Next visual steps (see PLAN.md Phase 3)

- Subclass `AHearthAgent` as `BP_HearthAgent`, assign a skeletal mesh + AnimBP, set it in a
  `BP_HearthGameMode`'s `AgentClass`. Meshes: Epic's free Fab characters
  (https://www.fab.com/sellers/Epic%20Games?listing_types=3d-model&categories=characters-creatures)
  are rigged to the UE5 skeleton already. Sketchfab (https://sketchfab.com/tags/blender) has more
  variety but needs rigging/retargeting. Give each of the six a visibly different look; a
  per-agent mesh map (agent id → mesh) in `BP_HearthGameMode` is the simple way.
- Subclass `AHearthLocation` as `BP_HearthLocation`; implement `OnFireChanged` (Niagara fire),
  `OnShelterChanged` (swap in a lean-to mesh).
- Drive the Directional Light's rotation from `Bridge->WorldTime` for day/night.
