# AuraProj Agent Workflow

## Visual change archive

For every completed job that changes project source, content, configuration, or documentation, create and archive a visual summary before reporting the job complete. The summary should make the behavior before and after the change understandable to a human without reading the diff.

Required completion steps:

1. Create a readable diagram, preferably an SVG for technical flows, in `Docs/Reports/Change-Archive/` using the name `YYYY-MM-DD-<short-slug>.svg`.
2. Include the affected flow, the important guard or behavior change, and the relevant validation/tests. Use a raster illustration only when it communicates the change better than a precise diagram.
3. Add a matching Markdown record beside it with the same date and slug. Record the intent, changed behavior, tests, and a link to the illustration.
4. Append the entry to `.claude/memory/visual-change-archive.md` so the project retains a searchable history.
5. Link the archived illustration in the final response.

Do not mark an implementation or fix job complete until the diagram, archive record, and memory index entry exist. Keep earlier archive entries immutable; add a new dated entry when a later job changes the same area.

