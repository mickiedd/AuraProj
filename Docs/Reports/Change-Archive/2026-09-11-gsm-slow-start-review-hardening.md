# Native GSM slow-start review hardening

## Intent

Independently verify the 2026-09-11 slow-start review packet and apply only confirmed follow-up fixes.

## Verified findings

- The finite-value guard and `std::chrono::steady_clock` were already present; added explicit integration coverage for `NaN`, `+Infinity`, `-Infinity`, and overflowing `1e309` input.
- Added stale-generation readiness coverage: a delayed nonce from launch A cannot ready replacement launch B.
- Added final-startup-deadline coverage: readiness arriving after cleanup is rejected deterministically.
- Added requester-disconnect coverage proving connection teardown does not close a healthy `starting` child.
- Confirmed status already exposes both configured deadlines and elapsed uptime.

## Applied fixes

- Native GSM now marks the short request-expiry response with `retryable: true` while preserving the owned child, job, and nonce.
- `UGameServerClient` parses the retryable contract.
- Login-to-Loading flow automatically retries retryable manager responses with a one-second backoff without returning the player to Login; terminal manager errors still follow the existing failure/fallback policy.
- Documentation now describes top-level deadline configuration and the retry contract.

## Validation

- Release CMake build: `AuraGSM.exe` and `GSMFixture.exe` built successfully.
- `python -B Tools/GSM/tests/integration.py`: 15/15 passed.
- `python -B Scripts/test_game_server_manager.py`: 5/5 passed.
- `python -B Scripts/test_gsm_request_deadline.py`: 5/5 passed.
- `git diff --check`: passed.
- UnrealBuildTool compiled `GameServerClient.cpp` and `LoginPlayerController.cpp` successfully. An initial link was blocked by the open editor, then a subsequent 6-action UBT pass linked `UnrealEditor-Aura.dll`, `UnrealEditor-AuraEditor.dll`, and `UnrealEditor-AuraAbilityGraph.dll`; a final up-to-date AuraEditor build exited 0. The final retry-handler lifetime guards also compile, but their DLL replacement is held by the still-open editor.

## Scope decision

The review's monotonic-clock concern is not a defect in the current source. The 900-second ceiling is observable through `/api/status` and remains configurable at the top level. The crash dumps remain unrelated to this GSM lifecycle fix based on their different engine/build provenance and signatures.

## Visual evidence

- [Before/after lifecycle and validation diagram](2026-09-11-gsm-slow-start-review-hardening.svg)
