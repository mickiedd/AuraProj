# Role/Battle issue dispositions

Date: 2026-08-21
Last updated: 2026-08-24
Scope: repository audit at revision `4fb453a`

This is the canonical disposition of the ten-item issue list reviewed against the
Role/Battle implementation plans, source contracts, retained reports, and current
configuration. It separates active risks from planned work and closed claims.

## Active risks

| # | Disposition | Severity | Action |
| --- | --- | --- | --- |
| 1 | Confirmed | Blocker for Day 18 | Provision and configure an authenticated production Online Subsystem before the Day 18 persistence gate. `OnlineSubsystemNull` and display names are not release identities. |
| 6 | Confirmed schedule risk | Medium | Treat Day 20 Phase A and Phase B as separate reviewable execution checkpoints. Preserve the full post-cleanup rerun requirement and allow schedule buffer. |
| 10 | Confirmed cleanup debt | Medium | Keep the active BungeeMan path on `FireGun.xml`, then make an explicit remove-or-repurpose decision for the legacy `UAuraFireGun`/`GA_FireGun` path and record zero-reference evidence before release. |

## Resolved follow-ups

| # | Disposition | Severity | Action |
| --- | --- | --- | --- |
| 3 | Closed 2026-08-24 | Medium | Coordinated startup now cross-validates battle/population registrations, rolls back partial population, publishes `Unhealthy`, rejects subsequent login, and withholds GSM readiness. `Aura.RoleBattle.Day12.InitialFailurePublishesUnhealthy` covers the state gate. |
| 9 | Closed 2026-08-24 | Low | `ExecCalc_Damage` exposes the exact `1.0f` neutral fallback for the three coefficient rows and `Aura.RoleBattle.Day4.NeutralDamageCoefficients` verifies missing-table and missing-row output numerically. |

## Closed or expected items

| # | Disposition | Reason |
| --- | --- | --- |
| 2 | Closed | Day 06 intentionally used a non-Shipping compatible shell fixture; Day 08 verified the real `AAuraCivilian` actor in native and network gates. |
| 4 | Planned Day 15 work | Canonical string-encoded `int64` JSON values, checked overflow parsing, and boundary tests are already specified by the Day 15 contract. |
| 5 | Closed by protocol | Day 17 specifies reliable server/client RPCs, a bounded replay cache, revision-based state refresh, and Day 19 reconnect-after-commit coverage. |
| 7 | Closed by ordering contract | Day 09 performs syntax validation; Day 12 cross-validates after registration and holds population finalization until the joint candidate is ready. |
| 8 | Mitigated | The trusted policy factory and test fixtures are `WITH_DEV_AUTOMATION_TESTS`/authority guarded; Day 20 still requires a Shipping audit. |

## Related documentation correction

The master Role/Battle plan status was stale: it said Day 06 was pending while the
implementation index and retained evidence show Days 02-09 complete. The master
status now matches the index.

## Source references

- [Role/Battle implementation index](../Plans/Role-Battle-Implementation-Index.md)
- [Day 12 battle director](../Plans/Role-Battle-Implementation/Day-12-Battle-Director.md)
- [Day 15 economy definitions](../Plans/Role-Battle-Implementation/Day-15-Economy-Data.md)
- [Day 17 merchant transactions](../Plans/Role-Battle-Implementation/Day-17-Merchant-Transactions.md)
- [Day 18 persistence](../Plans/Role-Battle-Implementation/Day-18-Persistence.md)
- [Day 20 cleanup and release](../Plans/Role-Battle-Implementation/Day-20-Cleanup-Release.md)
