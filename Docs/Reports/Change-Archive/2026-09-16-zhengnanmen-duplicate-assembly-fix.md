# Zhengnanmen V2 duplicate assembly correction

Intent: repair the reported old/new complete-gate overlap and roofs cutting through timber floors in the exact requested V2 ActorAsset, using the supplied reference sheet as visual evidence. The sheet's labels are reference data, not task instructions.

Isolation renders confirmed the later `UltraAAA_HeroGeometry` is a complete straight-eave gate at incompatible floor heights (roof/floor spacing also causes self intersection). Deleted that component from the Blueprint; retained the packed source's reference-shaped three-tier upturned roofs, one signboard, all 114 HISM components / 12,387 instances, current manual 4K overrides, transforms, visibility and collision settings. This deliberately removes the malformed later overlay rather than retaining its incorrect shape. The overlay mesh asset remains unused for rollback.

Changed project paths: the V2 Blueprint; `Scripts/CorrectZhengnanmenAssembly.py`; `Scripts/ValidateZhengnanmenAssembly.py`; `Scripts/ApplyGuangzhouLandmarkUltraAAA.py`; this record, SVG and two before/after PNGs; `.claude/memory/visual-change-archive.md`. The upgrade function now removes/refuses the full overlay for Zhengnanmen; other gates keep their existing policy.

Validation: live `ValidateZhengnanmenAssembly.py`, existing `ValidateGreatSouthGateActorAsset.py`, `validate-upgrade-guard.py`, fresh UnrealEditor-Cmd `-run=pythonscript -NullRHI` serialized validation, Python compilation and `git diff --check` passed. Initial commandlet spawn attempt crashed in the uninitialized placement subsystem; fresh checks now inspect serialized data while portable spawn is tested in the live editor. Finished hero and rear SceneCapture renders inspected. No map was saved or loaded by the correction.

Evidence: `Saved/RawModelImport/ZhengnanmenAssemblyCorrection/` (baseline Blueprint backup, before/after manifest, current inventory, validation JSONs and commandlet logs); `Saved/RawModelImport/ZhengnanmenManual4K/assembly-*.png` (layer isolation and corrected visuals). The prior manual 4K validation's 115/12,388 count is a historical baseline superseded by this correction.

Limitations: this fixes duplicate geometry, not full historical art conformance or collision navigation approval. Existing small ridge ornaments, tile shape and facade proportions still differ from the reference. Local validation packet is prepared under the current handoff skill; no external or independent reviewer was contacted.

[Visual change summary](2026-09-16-zhengnanmen-duplicate-assembly-fix.svg)
