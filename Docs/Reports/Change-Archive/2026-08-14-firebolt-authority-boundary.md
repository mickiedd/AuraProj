# FireBolt authority completion boundary — 2026-08-14

## Intent

Prevent a predicted client from ending the FireBolt ability before the dedicated server reaches the authoritative projectile-spawn node.

## Changed behavior

- `SpawnProjectiles` still skips its side effect on non-authority instances, but now logs the handoff explicitly.
- A successful graph completion on a predicting client is held until the authoritative server replicates the ability end.
- The same guard covers graphs that complete immediately during `ActivateAbility()`.
- Graph failures and montage interruptions still cancel immediately; only successful non-authority completion waits.
- The server remains the sole owner of FireBolt projectile/effect creation.

## Validation

- `python Scripts/test_firebolt_graph.py` passed.
- `AuraAbilityGraphSmokeTest` passed: 26 checks, 0 failures, including `ClientGraphCompletionBoundary`.
- `AuraEditor Win64 Development` build passed.
- Full Windows cook passed: 3,304 packages.
- Full Windows Server cook passed: 3,304 packages.
- `git diff --check` passed; only normal Git LF/CRLF conversion warnings were reported.
- Dedicated-server source compilation remains unavailable in the installed prebuilt Unreal distribution (`Server targets are not currently supported from this engine distribution`); a source-built Unreal engine is required to rebuild the server executable with this code.

## Visual summary

[View the FireBolt authority-boundary diagram](./2026-08-14-firebolt-authority-boundary.svg)
