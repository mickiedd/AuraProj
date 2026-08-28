# Role/Battle review fixes and submission gate

## Intent

Apply the deep review fixes for the pending Role/Battle change set and verify that the resulting code is ready for submission.

## Changed behavior

- Player and world state now commit through one verified checkpoint manifest for persistent purchases; failed persistence restores wallet, inventory, stock, and revisions.
- The one-shot login profile is cleared after successful initialization, incomplete profiles are not saved on logout, and listen-server readiness bypass is limited to initialization rather than unhealthy startup.
- Dormant merchant stock is retained and restored when the population slot respawns; merchant death closes its authoritative presentation.
- Broom mount, dismount, yaw, and flight requests validate the controlled rider, range, finite input, and normalized/clamped values. The mount component no longer exposes an unscoped client RPC bypass.
- Smoke runs select the explicit development provider and isolate per-run save fixtures; editor executable routing uses an exact allowlist.
- Zero-priced economy definitions are rejected at load time, and the WebUI exposes the persistence-failure result code.

## Validation

- All `Scripts/test_*.py` contract tests passed.
- Python, JSON, XML, and PowerShell validation passed for the active project files; the legacy `build.ps1` batch-file contents were excluded from PowerShell parsing.
- AuraEditor Win64 Development build passed after the final readiness and broom-routing changes.
- Previously completed runtime matrices passed for Day 16 Listen/Dedicated, Day 17 Listen/Dedicated, Day 18 Listen, and Day 19 Listen; the final surgical routing changes were additionally compile- and contract-tested.
- `git diff --check` passed.

## Submission note

The code review has no remaining blocking findings for the intended local/LAN development scope. `GameServerManager.py` remains a deliberately unauthenticated LAN bridge and should be firewall- or loopback-restricted before exposure to an untrusted network. The pending repository contains many untracked implementation, test, documentation, and archive files; stage only the files intended for this CL.

## Illustration

[Review fixes and submission gate](./2026-08-28-role-battle-review-fixes-submit-gate.svg)
