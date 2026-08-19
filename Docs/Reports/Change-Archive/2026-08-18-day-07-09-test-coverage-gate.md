# Day 7-9 test coverage gate

## Intent

Make the three-day verification gate trustworthy by checking the exact plan inventory, requiring native automation results in the logs, and expanding the Civilian network probe assertions.

## Changed behavior

- The static contract script now requires all 25 named Day 7/8/9 automation paths and all five topology runners.
- The native runner quotes Unreal automation arguments correctly and fails unless the log contains the expected successful-test count and `EXIT CODE: 0`.
- Day 8 network checks now require server Civilian identity/ASC initialization and an empty offensive grant set, plus matching client presentation.
- Day 9 network checks now require all three canonical slots, volume registration, late-client replication of all six member fields, no refill/respawn marker, and stable duplicate finalization.

## Validation

- AuraEditor Win64 Development build passed after the GameMode probe change.
- Native automation passed: Day 7 `10/10`, Day 8 `6/6`, Day 9 `9/9`; listen and dedicated Day 7 wrappers also passed.
- Day 8 Listen and Dedicated reports passed with server identity/loadout and both client presentation assertions.
- Day 9 Listen and Dedicated reports passed with `3/3` server slots, `3/3` client members, late-join state, duplicate initialization stability, and no refill.
- Static contracts, PowerShell syntax, and `git diff --check` passed.
- Packaged BuildCookRun remains explicitly unpassed because the corrected client-target cook exceeded the bounded engine compile window; no packaged result is inferred.

## Illustration

[Open the Day 7-9 test coverage gate](2026-08-18-day-07-09-test-coverage-gate.svg)
