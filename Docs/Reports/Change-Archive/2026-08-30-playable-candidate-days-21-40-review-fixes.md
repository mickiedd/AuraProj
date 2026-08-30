# Playable Candidate Days 21–40 review fixes

## Intent

Re-review the newly implemented Days 21–40 work, distinguish real defects from missing external evidence, repair confirmed runtime and tooling bugs, and prevent local/static checks from being reported as packaged multiplayer completion.

## Changed behavior

- Persistent role defaults are applied before completed profile state, so saved firearm ammunition is no longer overwritten during spawn. Failed player/world manifest commits now restore the prepared profile and delete uncommitted record slots.
- Projectile class resolution, deferred construction, definition configuration, and damage setup precede authoritative ammunition consumption. A missing firearm authority fails closed. Death cancels reload before the life-state transition, and reload start rejects a non-alive pawn.
- Tutorial progress is driven by server-observed movement, interaction, ability commit, purchase, and spawn/recovery events; the owner-only replicated mask now broadcasts through `OnRep` and refreshes the WebUI HUD.
- Diagnostics require stable run/build/session/server/world/map/topology/role/result correlation and a caller-supplied run-scoped HMAC key of at least 32 bytes. Raw identity and secret values remain redacted.
- Filtered Fast runs are diagnostic and nonzero. Candidate/External, packaged visual QA, and Day 39 soak remain `BLOCKED` until real package and lane evidence exists. Day 40 requires explicit artifacts bound to the same run, source revision, scope/content hashes, package SHA-256, exact four passing lanes, at least ten cycles per lane, current HEAD, and a clean working tree.

## Validation

- `AuraEditor Win64 Development` built successfully with Unreal Engine 5.5.
- `Scripts/test_playable_candidate.py --days all`: 20/20 passed.
- Native `Aura.RoleBattle.Day21` through `Day40`: 20/20 passed across the suite run and focused Day 38 rerun.
- All six changed PowerShell entry points parsed successfully.
- Full Fast Both/Both returned `PASS`/exit 0; filtered Fast returned `DIAGNOSTIC`/exit 2; Day 39 without packaged evidence returned `BLOCKED`/exit 2.
- Diagnostics probe produced schema 2, a 64-character identity HMAC, redacted raw identity, and no raw identity leak.
- `git diff --check` reported no whitespace errors (only existing line-ending warnings).

Independent in-app ChatGPT review could not be dispatched because no existing ChatGPT tab was available on either bounded discovery attempt. No repository data was transmitted; the successful compile and local/native checks were used as the documented fallback.

## Remaining gates

Packaged listen/dedicated execution, the four named lanes, ten-cycle soak evidence, visual captures, and production-provider validation are still explicitly blocked rather than claimed complete.

## Illustration

[Playable Candidate Days 21–40 review-fix flow](2026-08-30-playable-candidate-days-21-40-review-fixes.svg)
