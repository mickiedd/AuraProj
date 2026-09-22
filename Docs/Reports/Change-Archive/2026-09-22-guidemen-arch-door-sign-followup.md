# Guidemen arch, door and English sign follow-up

The saved `BP_Guidemen_V5_4K_PreRebuild_20260918` Blueprint was tuned against the supplied close-up reference.

- Replaced the thin arch edge with a private `SM_ArchSpandrel_VoussoirRevisionRoll` containing 17 closed stone voussoirs around the 280 cm opening while retaining the tunnel opening.
- Rebound both door leaves to `M_GateDoor_WeatheredReference`, using the authored gate textures with a restrained brown tint and visible grain.
- Replaced the clipped plaque map with a task-owned weathered texture carrying the complete `PORTE DE GUIDE` / `VILLE DE CANTON` wording, and raised the plaque center to 750 cm for a clear gap above the arch.
- Kept source downloads unchanged; all new assets live under `Content/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/ReferenceRepair20260921_ArchDoorSign`.

Validation: 17 arch clearance rays measured 279.03 cm, the serialized actor reloaded with 47 components and preserved non-target instance counts, and a fresh Unreal capture confirmed the arch, door and plaque visually. The transient capture actor was cleaned up without saving a map.

[Change illustration](2026-09-22-guidemen-arch-door-sign-followup.svg)
