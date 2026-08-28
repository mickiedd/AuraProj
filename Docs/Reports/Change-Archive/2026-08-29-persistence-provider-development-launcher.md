# Persistence provider development launcher fix

## Intent

The latest server log showed the client connecting with provider `null` while the authority server expected `Steam`, producing `Network error (code 8)` during `PreLogin`. The fix makes the development provider choice explicit at the launcher boundary.

## Changed behavior

- `Scripts/GameServerManager.py` accepts a development provider override and forwards it to each managed dedicated server as `-AuraPersistenceProvider=<name>`.
- `StartGameServer.bat` and `StartGameServer.command` default the local development launcher to `NULL`, matching the local WebUI/OnlineSubsystemNull client harness.
- `AURA_PERSISTENCE_PROVIDER=Steam` remains available for local Steam-backed testing.
- The project default remains `ExpectedProviderName=Steam`; the runtime override remains unavailable in Shipping builds.
- The macOS launcher now passes arguments through an array, keeping provider and executable paths from being reinterpreted by shell evaluation.

## Validation

- All 15 `Scripts/test_*.py` contract scripts passed.
- The Game Server Manager launch contract verified the exact `-AuraPersistenceProvider=NULL` argument.
- `python -m py_compile` passed for the changed Python files.
- CLI help exposed `--persistence-provider`.
- `git diff --check` passed.

## Operational note

Stop and restart the existing Game Server Manager and any dedicated server it started so the new command line is applied. Launch through `StartGameServer.bat` on Windows or `StartGameServer.command` on macOS. Use `AURA_PERSISTENCE_PROVIDER=Steam` only when the client and server are both configured for the authenticated Steam provider.

Illustration: [development provider launcher flow](2026-08-29-persistence-provider-development-launcher.svg)
