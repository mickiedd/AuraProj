# Zhengdongmen / Great East Gate imported as a landmark Blueprint

**Date:** 2026-09-25
**Illustration:** [2026-09-25-zhengdongmen-landmark-import.svg](2026-09-25-zhengdongmen-landmark-import.svg)
**Materials bound:** [2026-09-25-zhengdongmen-materials-bound.png](2026-09-25-zhengdongmen-materials-bound.png) — all 7 groups × 6 channels as actually imported

## Intent

The user supplied two zips — `Zhengdongmen_Great_East_Gate_Procedural_UE5_Package.zip`
(110 MB) and `Zhengdongmen_Great_East_Gate_Compact_8K_Materials.zip` (84 MB) — together
with a reference board, and asked for the Great East Gate (大东门 / 正东门) to be imported
into the project **as a building blueprint**.

Zhengdongmen did not previously exist in the landmark library. The project does carry a
large amount of `Dadongmen`-named work (`AnalyzeDadongmenSource.py`, `ApplyDadongmenDeepAAA.py`,
`CaptureDadongmen*.py` and ~35 more), but that is the retired **V4** generation: `find
Content -iname "*adongmen*"` returns nothing, there is no `V4/` folder under
`GuangzhouLandmarks/`, and those scripts are the V4-era captures that the 2026-09-22 notes
already flagged as writing no output at all. So this is a fresh import, not a revival.

## Changed behaviour

Before: no Zhengdongmen assets anywhere in `Content/`. After: the landmark library gains a
complete, placeable gate, and the showcase ring goes from 8 landmarks to 9.

```
/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/
  BP_Zhengdongmen             PackedLevelActor, 7 HierarchicalInstancedStaticMeshComponents
  Meshes/                     7 x SM_ZDM_<Group>
  Materials/                  M_ZDM_Master + 7 x MI_<Group>
  Textures/                   42 x T_ZDM_<Group>_<Map>
```

| | |
|---|---|
| Triangles | **50,442** — the declared LOD0 count exactly |
| Footprint | **2870 × 1639 × 1933 cm** — the union of the source GLBs exactly |
| Import transform | roll **−90°**, uniform scale **1.0** |
| Material groups | Stone, Wood, RoofTile, Plaster, Iron, DoorWood, Sign |
| Ring radius | 10254.19 → **11812.90 cm**; every existing landmark moved (arithmetic, not a regression) |

### Two packages, one geometry — the choice is purely artwork

Both zips ship the **same mesh**. That was verified by reading both GLBs' POSITION
accessors rather than trusting the READMEs: identical 7 meshes, identical 50,442 triangles,
identical bounds. They differ only in texture artwork, so the decision reduces to which
artwork to bind.

The Compact set wins on inspection: real masonry courses with mortar and weathering, curved
ceramic roof-tile profiles, timber grain, weathered limewash, and a timber door with iron
studs and ring handles — against the Procedural set's flat procedural swatches.

### The 8K BaseColor is an upscale, so it is staged at 4K

The Compact package ships BaseColor at 8192², but its own README says those were upscaled
from ~1254 px illustrations. That was measured, not taken on faith: an 8K → 4K → 8K round
trip reproduces the 8K to a **mean absolute error of 0.49–0.62 of 255** — JPEG-noise level —
and even a 2K round trip stays under 0.93. The 8K therefore carries no detail above ~2K, and
4K already oversamples the source by 3.3×.

`Scripts/PrepareZhengdongmenPackage.py` stages BaseColor at 4096 and ships the Compact 1K
support maps as-is. **Pairing them with the Procedural package's 4K normals would have mixed
two different illustration sets on one material**, so the whole set stays Compact.

### Why the single GLB is split into seven

`Zhengdongmen_LOD0.glb` is one file holding seven meshes, one per material. Imported
combined it becomes one StaticMesh with seven slots whose order is whatever Interchange
produced. Split into one GLB per material it becomes seven StaticMeshes with exactly one
slot each, so **the material binding is by construction** — the same reasoning
`ImportZhengximenLandmark.py` used on that package's modular GLBs.

The split keeps the source buffer, accessors, bufferViews, images and materials untouched
and prunes only the scene graph, so every retained mesh is byte-identical to the source.

## The finding: the import negates Y, so the facade lands on +Y

This is the substance of the job, and it is the reason `ValidateZhengdongmenLandmark.py`
exists.

The **source** model puts its facade on local **−Y**: the generator shipped with the package
(`Scripts/build_gate.py`) authors the signboard at `Y -5.97..-5.89` with its full-UV front
face at `Y -5.982`, and the README states `front = -Y`.

The **import** negates Y, so the facade arrives on local **+Y**. Measured on the imported
meshes; three parts agree:

| part | source GLB local Y | imported local Y |
|---|---|---|
| Iron (door studs) | +204 … +211.5 | **−211.5 … −204** |
| Sign (the 正东门 plaque) | −598.2 … −589 | **+589 … +598.2** |
| Stone (base centre) | −4.5 | **+4.5** |

### Why no other import roll can fix it

The GLBs are trimesh output with **Z-up POSITION values in a Y-up container**, so Interchange
applies its Y-up-to-Z-up conversion and then the import roll. The composition at roll −90 is
`(x, y, z) → (x, −y, z)`. All four rolls about X were derived:

| roll | result | verdict |
|---|---|---|
| 0 | `(x, −z, −y)` | height on Y — wrong |
| **−90** | `(x, −y, z)` | correct up-axis, **facade on +Y** |
| +90 | `(x, y, −z)` | height on Y — wrong |
| 180 | `(x, z, y)` | height on Y — wrong |

Only roll −90 puts the building's height on Z, and it necessarily yields the Y negation. The
negation is therefore structural to this pipeline, not a setting to tune.

### The trap: a size check cannot see a mirror

**A mirror preserves every extent.** `(x, −y, z)` and `(x, y, z)` produce identical X/Y/Z
sizes, so the adaptive import test — which judges each candidate against the GLB's own
POSITION bounds — reported `max_deviation` within 2% and a perfect match while the gate was
standing back to front. The same blind spot exists in `ImportZhengximenLandmark.py`.

Only a **signed, asymmetric feature** distinguishes them, and this model has exactly three:
the plaque, the door studs, and a 9 cm asymmetry in the stone base. Everything else is
symmetric about XZ, which is why the defect is invisible in a bounding box and was caught
only by the facade check.

## The fix

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — Zhengdongmen `facing_offset 0.0 → 180.0`,
  with the three readings in the comment. It is now the second landmark with a non-zero
  offset, for the **opposite reason to Wenmingmen**: there the model itself faced +Y, here
  the model faced −Y and the import flipped it.
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — `FACADE_LOCAL_AXIS["Zhengdongmen"] =
  (0.0, +1.0)`, and `Landmark_Zhengdongmen` added to `EXPECTED_LABELS` (the light labels
  derive from that list, so the two move together).

The mirror is otherwise immaterial: every part except the plaque, the door and that 9 cm is
symmetric about XZ. It does **not** mirror the plaque text — the quad's in-plane axes are X
and Z, both of which the import preserves, so 正东门 still reads correctly from the new front.

## Validation

Import (`Scripts/ImportZhengdongmenLandmark.py`): `passed: true`, `problems: []`, 7/7 groups
all at roll −90 / scale 1.0 on the first candidate, 42 textures, 7 instances, union
2870 × 1639 × 1933 cm, 50,442 triangles.

Landmark validation (`Scripts/ValidateZhengdongmenLandmark.py`, fresh process):
**0 errors / 0 warnings** — every mesh one slot bound to its own `MI_*`, Nanite on with the
full `PERCENT_TRIANGLES` fallback, structural groups carrying `CTF_USE_COMPLEX_AS_SIMPLE`,
every mesh double-sided as the source declares, all source GLBs carrying `TEXCOORD_0`, all
42 textures with the right colour space, all normals with `flip_green_channel` set, the
Blueprint a `PackedLevelActor` with 7 HISMs on a `LevelInstanceComponent` root spawning to
the union footprint, and the facade re-measured on +Y.

Ring validation (`Scripts/ValidateGuangzhouLandmarkShowcase.py`, fresh process):
**0 errors / 0 warnings** — 9 landmarks / 9 lights / 9 labels, 0 XY overlaps, 0 light
isolation violations, angular gaps 31.808°–48.465°, and **every facade 0.000° off the
plaza** including Zhengdongmen. Zhengdongmen grounds at world Z 0.0 with height 1933.0 cm,
ring angle 336.311°, radius 11253.88 cm, its own shadow-casting spot light attached with
0.0 cm location error, 0.0° aim error and a 17.253° nearest-neighbour cone margin.

### Bugs the run found in my own work

- The import report's `union_size_cm` used **max-per-axis extent**, which under-reported the
  height as 1663 cm against the true 1933 cm — the exact trap the Zhengximen record already
  documents, and I reintroduced it. Fixed to union the absolute bounds; the report now
  agrees with the source to the centimetre.
- `Scripts/ValidateZhengdongmenLandmark.py` was written expecting the facade on **−Y**, from
  the package documentation. The first run failed with the plaque at `Y 589.00..598.20`. The
  validator was right and the expectation was wrong — which is the whole point of measuring
  rather than assuming, and the reason the check is worth having.

## Deliberate choices

- **Green channel IS flipped**, unlike Zhengximen. The Compact package's README states its
  generated normals use the glTF/OpenGL +Y convention, which is what `flip_green_channel`
  converts; Zhengximen's package documented DirectX (Y−). The maps are not flat — measured
  std of (green−128) is 5–20 of 255 — so the direction is a real visual choice.
- **Nanite on all seven groups** with `fallback_target = PERCENT_TRIANGLES` at 1.0 and zero
  relative error, matching the project convention. `AUTO` decimates the fallback and renders
  black triangular holes on Metal.
- **`two_sided` on the master material and `double_sided_geometry` on every mesh.** The
  source glTF declares every material `doubleSided=True`, and the geometry relies on it: the
  plaque is a single quad and the window recesses are open shells.
- **`HeightTex` is exposed but left unconnected**, as with `M_Zhengximen_Master`. The height
  maps are 1K estimates and the project has no parallax-occlusion convention, so the
  parameter exists for a later tuning pass rather than being wired blind.
- **The split GLBs are staged, not committed.** Same open question as Zhengximen's 280 MB of
  `Raw3DPacket/` source.

## Still open

- **Native visual capture is PENDING.** Everything above is structural and does **not** prove
  the gate looks right; rendering needs an editor with a live RHI, and a commandlet has none.
  Remote execution still cannot discover the editor.
- Both packages' READMEs state the dimensions are proportional art-reconstruction estimates,
  so comparison against the reference board is a visual judgement, not a measurement. The
  Procedural README is explicit that the package is "a usable procedural reference/blockout
  package, NOT a complete commercially publish-ready AAA asset": roof tiles are simplified
  box-like strokes rather than accurate interlocking curved ceramic profiles, and no UV1
  lightmap channel is supplied (the project lights with Lumen, so it is not on the critical
  path).
- The door studs' ironwork is a texture on the DoorWood material rather than separate
  geometry, so its metalness is approximate.
