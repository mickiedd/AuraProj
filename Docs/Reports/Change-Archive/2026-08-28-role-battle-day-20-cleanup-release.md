# Role/Battle Day 20 - Cleanup and Release Verification

Date: 2026-08-28

## Intent

Remove transitional combat, lifecycle, and development mutation assumptions, finish the FireGun-path audit, and verify the post-cleanup vertical slice deeply enough to identify any remaining release blocker.

## Changed behavior

- Pickup eligibility and melee damage now use shared combat policy access instead of local faction assumptions.
- Role grants use the existing reconciliation ledger; unsafe runtime grant helpers and legacy cheat/RPC mutation surfaces were removed or compile-gated.
- Civilian respawn uses deterministic collision-safe fallback offsets.
- Day 2-19 runners carry explicit world persistence IDs and the Day 17 Listen probe waits for two named remote clients before measuring trade behavior.
- Shipping packaging keeps development automation out of the release binary and stages the active JSON/XML/map inputs.
- The active BungeeMan grant remains `FireGun.xml`; the legacy `UAuraFireGun` path is documented as non-active compatibility code.
- The reusable Day 20 native, XML, and Python checks plus the economy schema and vertical-slice procedure were added.

## Validation

- AuraEditor, Aura, AuraServer Development, and Aura/AuraServer Shipping builds passed.
- Day 20 native automation passed 15/15; full `Aura.RoleBattle` passed 213/213. The historical `GA_MeleeAttack` snapshot is covered as non-active; `EnemyAbilityConfig.json` points to the XML `EnemyMeleeDamage` path.
- All 15 repository Python contract scripts passed.
- AuraAbilityGraph smoke passed 26/26; AutoTest `RunAll` passed 6/6 with no failures, timeouts, errors, or aborts.
- The post-cleanup Day 2-19 Listen/Dedicated sweep passed, including Day 19 emulation and bounded performance thresholds. Day 7 packaged Development topology passed with matching server/client network checksums.
- The clean Shipping package passed UAT and staged-data/privacy scans: no PDBs, test reports, credentials, grant/cheat strings, or generated runtime files were present. UDP launch sanity passed on port 20095.
- Deep source, XML, JSON, SVG, and diff review passed; only existing LF-to-CRLF normalization warnings appeared in `git diff --check`.

## Disposition

The local implementation and all executable local test cases are green. The mandatory production Online Subsystem and two-account packaged Shipping matrix could not be executed because this environment has no provisioned production provider/App ID and no two authorized release accounts. Null/fixture identities are not acceptable substitutes. Consequently the Day 20 completion gate is **BLOCKED BY EXTERNAL PROVIDER PROVISIONING**. The source cleanup is ready for that preflight, but the milestone must not be represented as release-complete until it passes.

## Evidence

- Final report: `../Role-Battle-Vertical-Slice-2026-08-28.md`
- Focused native log: `Saved/Logs/Day20AutomationFocused-Expanded.log`
- Full native log: `Saved/Logs/Day20AutomationFull-Expanded.log`
- Plugin smoke log: `Saved/Logs/Day20-AuraAbilityGraphSmoke-Final.log`
- AutoTest log: `Saved/Logs/Day20-AutoTestRunAll-Final.log`
- Clean staged archive: `Saved/StagedBuilds/Day20ShippingRelease`
- Release contract: `Content/AutoTests/RoleBattleDay20Release.xml`

## Illustration

[Role/Battle Day 20 cleanup and release gate](2026-08-28-role-battle-day-20-cleanup-release.svg)
