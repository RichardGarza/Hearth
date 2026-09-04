#!/bin/zsh
# One command to play: starts Ollama (if needed) and the brain, opens the game window, and when the
# game closes (Quit in the ESC menu, or the window's close button) shuts the brain down so nobody
# keeps talking in the other room.
#
#   ./play.sh                      # Jonah on the local model, voices on
#   ./play.sh --brain scripted     # everyone scripted, free, no Ollama needed
#   ./play.sh --brain claude --ai-agents jonah --budget 3
#
# Extra arguments are passed to the brain. Voices: add --voice none to silence.
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8"
BRAIN_ARGS=(--brain ollama --ai-agents jonah --voice say --tick-seconds 6 --quiet)
if [[ $# -gt 0 ]]; then BRAIN_ARGS=("$@"); fi
BRAIN_ARGS+=(--exit-with-client)   # brain shuts itself down when the game disconnects

cleanup() {
  # the interpreter is called "Python" (capital P) on this Mac, so match on the module args only
  pkill -f "hearth run" 2>/dev/null
  pkill -x say 2>/dev/null
  echo "brain stopped."
}
trap cleanup EXIT INT TERM

if [[ " ${BRAIN_ARGS[*]} " == *" ollama "* ]] && ! curl -s -m 2 http://127.0.0.1:11434/api/tags >/dev/null; then
  echo "starting ollama..."
  (nohup ollama serve > /tmp/ollama_serve.log 2>&1 &)
  sleep 2
fi

mkdir -p "$ROOT/logs"
cd "$ROOT/brain"
.venv/bin/python -m hearth run "${BRAIN_ARGS[@]}" > "$ROOT/logs/brain-live.txt" 2>&1 &
sleep 2
echo "brain: ${BRAIN_ARGS[*]}"
echo "game: opening (ESC = menu, SPACE near [ AI ] = talk, Shift = run)"
"$UE/Engine/Binaries/Mac/UnrealEditor" "$ROOT/unreal/Hearth/Hearth.uproject" /Game/Maps/Valley -game -windowed -resx=1440 -resy=900 -log -ABSLOG="$ROOT/logs/unreal-live.log" > /dev/null 2>&1
echo "game closed."
