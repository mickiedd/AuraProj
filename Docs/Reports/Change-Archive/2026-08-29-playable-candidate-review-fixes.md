# Playable Candidate review fixes — 2026-08-29

## Intent

Close the deep-review issues found in commit `0a85dc4c0468e52b09f1463b89a7c7f7a7f56d88` without broadening the milestone.

## Changed behavior

- Added a separate loopback `gameServerAddress` to `ServerConnection.json`; `AURA_GSM_ADDRESS` is an explicit connect-endpoint override for LAN/public deployments.
- Added GSM request/readiness authentication, IP/CIDR allowlisting, fail-closed non-loopback startup, configured level/port checks, manager-owned public endpoints, and a per-launch readiness nonce inherited by each managed server.
- Updated both Unreal GSM senders to emit environment-backed credentials/nonces without logging them.
- Bound candidate finalization to explicit `DraftPath`/`SoakPath` inputs and matching run, scope, source, and package identities; reconciled the HMAC diagnostics contract in the plans and deep-review report.

## Validation

- `cmd /c build_test.bat` — AuraEditor Win64 Development build passed.
- All 16 `Scripts/test_*.py` contracts passed.
- Focused manager, editor-routing, persistence, and multiplayer tests passed.
- `git diff --check` passed.
- SVG/XML and JSON validation passed; generated tracked Python bytecode was restored after compilation.

## Illustration

[Before/after review-fix flow](2026-08-29-playable-candidate-review-fixes.svg)
