# Zhengximen live material repair

## Intent
Use the supplied gate reference and existing source textures; ensure the timber enclosure is continuous.

## Findings and changes
The workspace already contained UV and continuous-wall geometry repairs documented in [the earlier record](2026-09-25-zhengximen-reference-tuning.md). Those changes were preserved. Live Unreal inspection still showed checkerboard materials. The Metal log reported the master normal sampler was using Engine DefaultTexture (Color).

`ImportZhengximenLandmark.build_master` now repairs existing masters, assigns real source defaults to each texture parameter, uses Normal / Masks / Linear Grayscale samplers matching texture compression, and enables instanced-static-mesh and Nanite usage. `RepairZhengximenMaterialDefaults.py` applies this without reimporting geometry. Saved the repaired master and material instance changes in the live editor. The validator now checks parent source defaults and usage, in addition to instance bindings. Defaults are finalized after every node has a unique parameter name, avoiding editor synchronization of unnamed parameters. A fresh-process reload confirms all five connected defaults survive saving.

## Validation
- Native Blueprint viewport visually confirmed brown timber, dark tiles, gray masonry and the plaque instead of fallback checkerboard. [Native screenshot](../../../Saved/Reports/Zhengximen/native-material-repair.png).
- Source generator check-only: 8,800 wall rays over all four faces of both storeys, zero through-gaps. Existing gateway and door-seam checks also return zero open rays.
- Fresh UE 5.5 commandlet validation: exit 0, zero errors and warnings; saved Blueprint footprint 5320 × 1080 × 1689.68 cm, seven HISM components, texture bindings and UV checks pass.
- Live-world validator initially reported a 92 cm bounds discrepancy; the isolated fresh process passes. No geometry was changed to compensate for that live-world result.
- No claim of photorealistic equivalence to the reference; source textures and existing architecture are used.

## Illustration
[Before/after and validation diagram](2026-09-25-zhengximen-live-material-repair.svg)
