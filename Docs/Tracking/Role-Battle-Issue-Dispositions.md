# Role/Battle issue dispositions

Date: 2026-08-21  
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

## Narrowed findings

| # | Disposition | Severity | Action |
| --- | --- | --- | --- |
| 3 | Safety behavior is intentional; operational handling was underspecified | Medium | Keep all-or-nothing startup and fail-closed damage/population behavior. Add an unhealthy/reject-or-terminate startup outcome so a closed coordinator cannot look ready to clients. |
| 9 | Runtime fallback exists; its contract was undocumented and weakly tested | Low | Define the neutral coefficient as `1.0f` for every missing table/curve and add a numeric regression, not only a no-crash assertion. |

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
