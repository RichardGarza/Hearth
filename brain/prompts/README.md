# The AI's instructions — edit these

Plain text files loaded when the brain starts. Change them, restart, done. No code involved.

| File | Used for |
|---|---|
| `world_rules.md` | **The baseline.** Who they are as a group, the situation, what matters, how to behave. Sent to every model-driven character on every decision. `{ACTIONS}` is replaced with the generated action list. |
| `personas.toml` | **Who each person is.** Name, age, voice, body, background, traits, how they talk, private worry. Add or remove people here. |
| `converse_rules.md` | Extra rules when the visitor (you) talks to them. |
| `reflection.md` | How they compress old memories into notes to self. |
| `local_model_hint.md` | Appended for the small local model only (Ollama): JSON reminder, stay quiet. Claude doesn't need it. |

Voices: any name from `say -v '?'`. Bodies: `manny` (male) or `quinn` (female).
