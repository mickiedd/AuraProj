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
