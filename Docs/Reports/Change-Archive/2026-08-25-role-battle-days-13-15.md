# Role/Battle Days 13-15

Date: 2026-08-25

## Intent

Continue the Role/Battle plan through the Civilian lifecycle, player-owned targeting/interaction, and immutable economy-definition milestones without introducing a second Enemy respawn owner or trusting client presentation data.

## Changed behavior

- Day 13: `UAuraPopulationManager` now records explicit slot states, consumes exactly one neutral death event, retains a corpse for a policy duration, revalidates battle phase before scheduling and executing refill, reuses the canonical member ID, bounds retries, publishes debug snapshots, and cancels callbacks during shutdown.
- Day 14: targets expose independent relationship, kind, life, health, and interaction data. A replicated component on the owning PlayerController is the only interaction RPC endpoint and re-resolves requester, target, option, phase, distance, line of sight, replay order, and rate on the server. Interact uses its own action; LMB remains the ability/attack route.
- Day 15: items, offers, merchants, currency, capacity, and per-member merchant bindings load into one authority-only, immutable GameInstance snapshot. Canonical decimal strings are checked directly into `int64`; failed candidates never replace the published snapshot.
- Startup now requires the economy registry before initial population finalization. The shared Civilian role does not make every member a merchant.

## Validation

- `build_test.bat`: AuraEditor Development passed.
- `Scripts/test_role_battle_days_13_15.py`: passed; 28 named tests present.
- Native automation: Day 13 10/10, Day 14 11/11, Day 15 7/7.
- Full `Aura` automation: 154/154 passed, 0 failed.
- Network baselines: Day 13, 14, and 15 passed in Listen and Dedicated modes; each report contains eight passing assertions and two-client/late-join evidence.
- Economy and population JSON parsed successfully; `git diff --check` passed apart from line-ending notices.

## Illustration

[Role/Battle Days 13-15 flow](2026-08-25-role-battle-days-13-15.svg)
