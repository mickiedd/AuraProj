# Ignore local Raw3DPacket assets — 2026-09-10

Intent: unblock publication of the unpublished AuraProj commit while keeping the large source packets available in the local workspace.

Before: four `Raw3DPacket` ZIP archives were committed, including two files larger than GitHub’s 100 MiB blob limit.

After: `/Raw3DPacket/` is ignored, the archives are removed from the unpublished commit’s tree, and the prior ZIP-specific LFS rule is removed because these files are no longer tracked. Local files remain on disk.

Validation: the amended commit is checked for absence of `Raw3DPacket` paths and oversized outgoing Git blobs; the existing user-staged changes are restored after the amendment.

[Visual summary](2026-09-10-ignore-raw3dpacket.svg)
