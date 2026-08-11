# Last-seven-days change reference archive - 2026-08-12

## Intent

Replenish the recent change reference with a manual, detailed seven-day view modeled on the existing daily calendar coverage mind map. The window is 2026-08-06 through 2026-08-12, including today's explicit no-commit result.

## Changed documentation

- Added [Git-Daily-Changes-2026-08-06-to-2026-08-12.md](../Git-Daily-Changes-2026-08-06-to-2026-08-12.md), with daily commit groups, before/after behavior, changed surfaces, newcomer interpretation, and recorded validation evidence.
- Added the companion mind map [2026-08-12-seven-day-change-reference.svg](2026-08-12-seven-day-change-reference.svg).
- Added the recent supplement to the master daily mind-map index and this visual archive index.
- Earlier daily reports and archive illustrations remain unchanged.

## Coverage and validation

- Seven calendar dates are accounted for: six active dates and one quiet date.
- `git log main` reports 17 reachable commits in the window: `2 + 3 + 2 + 4 + 1 + 5 + 0` by date.
- The 2026-08-11 merge is split into the runtime beam/avatar branch and the runtime-asset Git LFS branch in the detailed reference.
- Existing Day 1-Day 4, beam, and LFS test evidence is linked rather than presented as newly rerun by this documentation-only job.
- The SVG is intended as the human-readable visual summary and parses as XML with the archive validation check.
