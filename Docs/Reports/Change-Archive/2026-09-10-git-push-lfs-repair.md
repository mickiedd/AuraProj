# Git push repair — 2026-09-10

Intent: unblock the unpushed Crunch asset commit without including unrelated staged work.

Before: four Raw3DPacket ZIP archives were ordinary Git blobs; two were 203,704,763 and 149,190,551 bytes, exceeding GitHub’s 100 MiB limit.

After: ZIP archives use Git LFS through `*.zip` in `.gitattributes`. Only the unpublished tip is amended; the original tip is retained at `refs/backup/push-repair-2026-09-10`. Existing staged work is preserved.

Validation: remote main was confirmed at the local tip’s parent. The repair script checks archive byte hashes, LFS pointer representation, outgoing Git blob sizes, and preservation of the staged diff. These checks must pass before the repair is reported as locally validated.

Push blocker: the LFS dry run cannot find GitHub credentials through the configured osxkeychain helper. Remote publication requires authentication and remains pending.

[Visual summary](2026-09-10-git-push-lfs-repair.svg)
