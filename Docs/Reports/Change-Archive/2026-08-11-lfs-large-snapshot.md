# Change archive: store the oversized Control Rig snapshot with Git LFS

Date: 2026-08-11

## Intent

GitHub rejected the local push because `Content/BungeeMan/CR_BungeeMan.snapshot.json` was a 192,874,753-byte ordinary Git blob, above GitHub's 100 MB per-file limit.

## Changed behavior

- Added an explicit Git LFS rule for `Content/BungeeMan/CR_BungeeMan.snapshot.json` in `.gitattributes`.
- Rewrote the three unpushed local commits with `git lfs migrate import`, excluding `origin/main` so the shared remote history remains unchanged.
- The committed representation is now a 134-byte LFS pointer. The full 183.94 MB snapshot remains available in the working tree after `git lfs checkout`.

## Validation

- `git lfs migrate info --include-ref=refs/heads/main --exclude-ref=refs/remotes/origin/main --above=100MB --pointers=ignore`: no ordinary Git blobs over 100 MB.
- `git lfs fsck --objects`: `Git LFS fsck OK`.
- `git status`: clean; `main` is 3 commits ahead and 0 behind `origin/main`.
- LFS pointer size: 134 bytes; referenced object size: 192,874,753 bytes.

The rewritten commits have not been pushed. `origin/main` remains an ancestor of the local branch, so a normal `git push origin main` can publish them; the Git LFS pre-push hook should upload the referenced LFS object.

Illustration: [2026-08-11-lfs-large-snapshot.svg](2026-08-11-lfs-large-snapshot.svg)
