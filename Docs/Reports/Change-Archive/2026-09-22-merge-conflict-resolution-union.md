# Merge of origin/main into main — append-only archive index resolved as a union

## Intent

`git pull` left the repository mid-merge with one conflicted path. Integrate
`origin/main` (`ac3200f`, "Add Canton 20-day terrain plans and hardening") into local
`main` (`a4c3973`, "Building asset updated.") without discarding either side's work.

## Changed behavior

- One conflicted path: `.claude/memory/visual-change-archive.md`.
- Both sides only **appended** entries at the tail of the file; neither side removed or
  edited an existing line. Verified against the merge base (`:1:`, 372 lines):
  ours (`:2:`, 396 lines) had 0 removed lines and 13 new entries; theirs (`:3:`,
  376 lines) had 0 removed lines and 3 new entries.
- Resolution is therefore the **union**: ours' 13 entries (2026-09-21 and 2026-09-22
  landmark work) followed by theirs' 3 entries (2026-09-22 test-suite contract repair,
  Canton terrain plan hardening, Canton terrain plan split) — chronological order
  preserved, nothing dropped.
- Resolved file: 400 lines, 87 entries. Pure LF (0 CR bytes), matching the previous
  blob; no binary or LFS-tracked asset was involved in the merge (73 staged files, all
  `.md`/`.cpp`/`.xml`/`.svg`/`.py`/`.ps1`/`.json`/`.h`/`.gitignore`).
- The merge commit `0ed042c6` is a true two-parent merge
  (`a4c3973e` + `ac3200f2`), not a fast-forward and not a squash.

## Validation

- Merge base, ours and theirs were each diffed against the resolved file: **0 removed
  lines in all three comparisons**, 16 additions against base (13 + 3).
- Conflict markers: none remaining.
- Entry counts: ours 84, theirs 74, resolved 87 (= 84 + 3), confirming the union added
  exactly theirs' three new entries on top of ours.
- Trailing bytes verified as `0a` with 0 CR bytes in the committed blob.
- `git status` reports a clean working tree and no `MERGE_HEAD`; all three of theirs'
  new entries and ours' final entry are present in the committed file.

## Notes

- The `git show <stage>:<path>` redirect wrote CRLF into the working file (core.autocrlf
  is `true` and `.gitattributes` sets `* text=auto`); this was detected by counting CR
  bytes and normalized back to LF before staging.
- The `post-commit` git-lfs hook was cut off by the shell timeout while the commit itself
  had already completed successfully — verified by inspecting the resulting commit, not
  by the exit status.

## Illustration

[Conflict and union resolution](2026-09-22-merge-conflict-resolution-union.svg)
