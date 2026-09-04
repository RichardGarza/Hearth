#!/bin/zsh
# End-to-end smoke test without a window: start the brain (scripted, free), launch the game with
# the null RHI, wait, then grep the Unreal log for the bridge connecting and agents spawning.
#   ./run_headless_test.sh [seconds=75]
set -u
SECS=${1:-75}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8"
PROJ="$ROOT/unreal/Hearth/Hearth.uproject"
LOG="$ROOT/logs/unreal-headless.log"

cd "$ROOT/brain"
.venv/bin/python -m hearth run --brain scripted --voice none --tick-seconds 4 --quiet --port 8765 > "$ROOT/logs/brain-headless.txt" 2>&1 &
BRAIN=$!
sleep 2

"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" /Game/Maps/Valley -game -nullrhi -nosound -unattended -nop4 -log -ABSLOG="$LOG" > /dev/null 2>&1 &
GAME=$!
sleep "$SECS"
kill $GAME 2>/dev/null; sleep 2; kill -9 $GAME 2>/dev/null
kill $BRAIN 2>/dev/null

echo "=== LogHearth lines ==="
grep -E "LogHearth" "$LOG" | sed 's/^\[[^]]*\]\[ *[0-9]*\]//' | head -40
echo "=== checks ==="
grep -q "Connected to brain" "$LOG" && echo "PASS connected" || echo "FAIL connected"
grep -q "Spawned 5 locations, 6 agents" "$LOG" && echo "PASS spawned" || echo "FAIL spawned"
grep -E "LogHearth: [a-z]+(->| -> )?.*: " "$LOG" | grep -v -E "Connect|Spawned|World init|Command" | head -1 | grep -q . && echo "PASS speech" || echo "FAIL speech"
echo "log: $LOG"
