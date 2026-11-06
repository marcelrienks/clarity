# Clarity: Claude project context

## Work plan

- `docs/todo.md` is the ordered work plan for the restructure specified in `docs/restructure.md` (source of truth for what and why).
- "Implement next todo point" means: take the first unchecked item in `docs/todo.md`, implement it, verify it, then tick it and add a short outcome note in the same style as the finished items.
- Item IDs (`D-`, `A-`, `C-`, `B-`, `T-`, `H-`, `X-`, `Q-`) refer to `docs/restructure.md`.
- Behaviour-changing items are gated on their `Q-` decision. Record outcomes in `docs/decisions.md`.
- Any change to a `[T]` log string updates the Wokwi scenario in the same commit.
- Do not commit or push unless asked. The draft PR (v2 to main) stays an unmerged draft until the todo is done.

## Build and test

- `pio` is not on `PATH`. Use `~/.platformio/penv/bin/pio` (or prepend `~/.platformio/penv/bin` to `PATH`).
- Envs: `release`, `debug-upload`, `debug-local`, `test-wokwi`, `test-wokwi-release`. Build with `pio run -e <env>`.
- Wokwi scenario suite: `bash test/wokwi/wokwi_test_automation.sh` (builds nothing; run `pio run -e test-wokwi` first). 5 parts, about 7 minutes in total, each under the 5 minute free-plan session cap.

## Wokwi token

- The Wokwi CLI token is stored in the macOS Keychain as a generic password with service `WOKWI_CLI_TOKEN`. The user has authorised using it for Wokwi runs.
- Export it for the command only, never print or log it, never write it to a file:

```bash
export WOKWI_CLI_TOKEN="$(security find-generic-password -s WOKWI_CLI_TOKEN -w)"
```

- Do not read `~/.wokwi/*.tok`; use the keychain item.
- Without the token every scenario part fails at 0 s with `Missing WOKWI_CLI_TOKEN environment variable`, which is not a firmware failure.
