# Visual Change Archive Memory

This project keeps a human-readable illustration and a short record for every completed implementation or fix job.

## Workflow

- Project rule: [AGENTS.md](../../AGENTS.md)
- Archive directory: [Docs/Reports/Change-Archive](../../Docs/Reports/Change-Archive)
- Preferred format: SVG for readable technical diagrams; use raster art only when it materially improves understanding.
- Every entry includes the before/after behavior, important validation, and test coverage.

## Archived illustrations

| Date | Job | Illustration | Record | Validation |
|---|---|---|---|---|
| 2026-08-11 | Safe damage actor validation and modular beam target fixes | [AuraDamageBeamFixes.svg](../../Docs/Reports/Change-Archive/2026-08-11-aura-damage-beam-fixes.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-11-aura-damage-beam-fixes.md) | SVG XML parse passed; 25 smoke tests passed, 0 failed |
| 2026-08-11 | Store oversized Control Rig snapshot with Git LFS | [large snapshot flow](../../Docs/Reports/Change-Archive/2026-08-11-lfs-large-snapshot.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-11-lfs-large-snapshot.md) | LFS fsck passed; no ordinary Git blob over 100 MB; local snapshot restored |
| 2026-08-11 | Manual daily Git diff and newcomer mind maps | [daily mind-map flow](../../Docs/Reports/Change-Archive/2026-08-11-git-daily-mind-maps.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-11-git-daily-mind-maps.md) | 172 main commits, 71 active dates, clean pre-change worktree, linked monthly reports |
| 2026-08-11 | Full calendar coverage for commit and no-commit days | [calendar coverage](../../Docs/Reports/Change-Archive/2026-08-11-daily-calendar-coverage.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-11-daily-calendar-coverage.md) | 117 calendar days accounted for: 71 active and 46 explicitly quiet |
| 2026-08-12 | Manual last-seven-days change reference with quiet-day coverage | [seven-day mind map](../../Docs/Reports/Change-Archive/2026-08-12-seven-day-change-reference.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-12-seven-day-change-reference.md) | 7 dates accounted for: 17 commits across 6 active dates and 1 explicit quiet date; SVG XML validation passed |
| 2026-08-12 | Seven separate daily SVG references | [daily SVG set](../../Docs/Reports/Change-Archive/2026-08-06-daily-change-reference.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-12-seven-daily-svg-references.md) | 7 standalone date diagrams; XML parsing and Markdown link validation passed |
| 2026-08-12 | July 11-August 5 daily SVG reference set | [daily SVG set](../../Docs/Reports/Change-Archive/2026-07-11-daily-change-reference.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-12-july-11-to-august-05-daily-svg-set.md) | 26 standalone date diagrams: 18 active and 8 quiet; XML and link validation passed |
| 2026-08-13 | Align FireBolt static test with native data-driven projectile definitions | [FireBolt test contract](../../Docs/Reports/Change-Archive/2026-08-13-firebolt-static-test-contract.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-13-firebolt-static-test-contract.md) | Python structural test passed; JSON native-class mapping validated |
| 2026-08-13 | Add a manual mind-map archive for all finished Role/Battle day plans | [finished DayPlan archive](../../Docs/Reports/Change-Archive/2026-08-13-finished-dayplan-mind-maps.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-13-finished-dayplan-mind-maps.md) | 4 finished-day SVGs rendered and XML-validated; links and diff checks passed |
| 2026-08-13 | Complete Day 05 versioned, validated, connection-scoped role registry | [Day 05 role registry](../../Docs/Reports/Change-Archive/2026-08-13-day-05-versioned-role-registry.svg) | [archive record](../../Docs/Reports/Change-Archive/2026-08-13-day-05-versioned-role-registry.md) | Build passed; 13/13 Day 05 tests; Day 05 and Day 04 Listen/Dedicated smokes passed; config/sentinel restored |
