# Hearth — Unreal side

`Hearth/` is a UE5 C++ project. It has never been compiled on this machine yet (no Unreal
installed). The code is a WebSocket *viewer* for the Python brain in `../brain`.

## What's in `Source/Hearth`

| File | Role |
|---|---|
| `HearthBridgeSubsystem` | GameInstance subsystem. Connects to `ws://127.0.0.1:8765`, parses frames, keeps latest `Locations`/`Agents`, fires `OnWorldInit`/`OnSnapshot`/`OnSpeech`/`OnEvent`. Auto-reconnects. `SendCommand("storm")` etc. for god mode. |
| `HearthGameMode` | Spawns one `AHearthLocation` per place and one `AHearthAgent` per person on `world_init`; on each snapshot walks agents to their target and updates labels. Default pawn is a spectator so you can fly around. |
| `AHearthAgent` | `ACharacter` with name / status / speech text above the head, moves via `AAIController::MoveToLocation` (needs a NavMesh). Blueprint events `OnSay`, `OnActionChanged`, `OnDied` for animation and audio. |
| `AHearthLocation` | Flat cylinder marker + label listing what's here. Point light turns on when the camp fire is lit. Blueprint events `OnFireChanged`, `OnShelterChanged` for particles/meshes. |
| `HearthTypes.h` | Blueprint-visible structs mirroring `docs/PROTOCOL.md`. |

## First-time steps (after installing UE5 + Xcode)

1. Right-click `Hearth.uproject` → *Generate Xcode Project* (or just open it; accept the rebuild prompt).
2. Open in the editor. Make a level: a Landscape or a large scaled cube as ground, a Directional
   Light, Sky Atmosphere, and a **NavMeshBoundsVolume** scaled to cover roughly 120 m × 120 m
   around the origin (`P` toggles nav display; it should be green).
3. Set the level as default map in Project Settings → Maps & Modes (or edit `Config/DefaultEngine.ini`).
4. Start the brain: `cd ../brain && .venv/bin/python -m hearth run --brain claude --voice say --tick-seconds 8`
   (slower ticks give characters time to walk at a believable speed).
5. Press Play. Output Log filter `LogHearth` shows connection and speech.

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
