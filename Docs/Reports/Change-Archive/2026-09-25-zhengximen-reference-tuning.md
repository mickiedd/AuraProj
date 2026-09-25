# Zhengximen tuned against the reference board — textures, continuous wall, arch-fitting door

**Date:** 2026-09-25
**Illustration:** [2026-09-25-zhengximen-reference-tuning.svg](2026-09-25-zhengximen-reference-tuning.svg)
**Follows:** [2026-09-25-zhengximen-landmark-import](2026-09-25-zhengximen-landmark-import.md)

## Intent

The user reviewed the imported gate against the supplied reference board and reported three
things: **"it should have textures, you can find out these materials in the source folder"**,
**"the timber's wall should be continuous, currently it's hollows"**, and then **"the wooden
door should fit the arc perfectly, no hollows"**.

**The reference board is interpretive, not a measured survey.** Its own README states the
dimensions are proportional art-reconstruction estimates, so the comparison below is a visual
judgement against the board, not a measurement.

## Changed behaviour

### 1. "No textures" was a mesh problem, not a material one

`M_Zhengximen_Master` and the eight `MI_*` were already correct, and the validator had
confirmed every texture parameter was bound. The **meshes** were the problem: every exported
GLB carried only `POSITION` and `COLOR_0` — **no `TEXCOORD_0` at all**. The generator computes
UVs in `add_box` / `add_cylinder_between` / `add_tube_curve` and stores them on the group, then
discarded them at export:

- `scene_from_builder(..., textured=False)` wrote `tm.visual.face_colors`, which *replaces* the
  texture visual, so the `uv` array was never written;
- the modular per-material export did the same.

`scene_from_builder(textured=True)` would have carried them, but the pipeline calls it with
`textured=False`. **So the package as shipped is untexturable through its own GLBs.** Both
paths now set `TextureVisuals(uv=np.array(g.uvs, float), material=PBRMaterial(...))`, and the
rebuild driver asserts `TEXCOORD_0` is present in every GLB it writes.

### 2. The hollow timber wall — three separate defects

| | |
|---|---|
| **0.12 m through-slot** | each bay was its own backing panel, `w = xb-xa-.12`, so a slot ran between every pair of bays |
| **No end walls** | the loop covered only `yy = ±lowerD/2` (front and rear); the two ends had no wall at all |
| **2.4 m above the gate** | doors stopped at `z = 3.22` under a vault springing at 3.40 and topping at 5.65 |

Fix: **one continuous closed wall per side on all four faces**, with the end walls running the
full depth past the front/rear faces so the corners close by overlap rather than on a hairline
seam. The lattice and rails stay proud of the wall, so each side still reads as joined timber
framing rather than blank boarding. The same treatment was applied to the upper storey.

### 3. Fitting the door to the arc

Three attempts:

1. **Plain boxes** — a rectangular door under a taller vault leaves a 2.4 m opening above the
   gate; the background showed straight through it.
2. **Horizontal slices** — closes it, but each step is a *chord* of the circle, so the leaf sat
   ~1.7 cm inside the opening where the arc turns vertical at the springing, and poked through
   the ring near the apex. 14 of 450 seam rays still open.
3. **Sampled strip** (`Builder.add_arched_leaf`) — columns across the leaf whose top edge *is*
   the arc, built to a radius `arch_pad = 0.02` larger than the opening so the leaf always
   overlaps it. The excess is hidden inside the arch ring. **0 open rays.**

Two further defects were visible only in a straight-on render, and both read as the door not
fitting even though the door was the larger of the two:

- **The wall's own opening was a staircase.** `slices = 46` gave 17 cm steps; once the door
  filled the arch, *that* staircase became the visible boundary. LOD0 now uses **400 slices**.
- **The barrel joints scalloped the edge.** The 3.5 cm vault tubes sat *at* radius `archR`, so
  their tips stuck into the opening and were the silhouette. Now flush (`archR + r`, r = 0.018).
  The voussoir ring also went 34 → 96 segments.

### 4. Plaque

The gate plaque mapped its texture by real-world size, so the single centred inscription
printed three times across it. `add_box` gained a `uv_unit` mode and the plaque now maps the
texture across its face exactly once.

## Tests

All verification ran as **isolated UE 5.5 Python commandlets**, each step followed by a
**separate fresh process** for validation.

### Source-side, by ray-casting

A bounding-box check cannot see the hollow-wall defect — the broken version still had panels.
The check that works fires rays perpendicular at each face and counts the ones that come out
the far side (`ContentSource/GuangzhouLandmarks/Zhengximen/source/build_zhengximen.py`):

| | before | after |
|---|---|---|
| wall, 8 faces | **4,840** open of 8,800 | **0** |
| gateway | **65** of 135 | **0** |
| door-to-arch seam | **242** of 450 | **0** |
| GLBs carrying `TEXCOORD_0` | 0 of 9 | **9 of 9** |

The `door_fit` check aims at fractions (0.995 / 0.98 / 0.96 / 0.93 / 0.90) of the arch
half-width at each height — *at the seam*, not the middle of the door, because a leaf that
only approximates the curve still looks solid in the centre. It stops 1% short of the apex:
the opening closes to a point there, so every fraction collapses to `x = 0` and the ray only
grazes the top edge, which is a sampling artefact rather than a gap.

### Project-side

| Step | Result |
|---|---|
| Reimport (`Scripts/ImportZhengximenLandmark.py`) | exit 0, `passed: true`, `problems: []`, 7/7 groups |
| Landmark validation (`Scripts/ValidateZhengximenLandmark.py`) | **0 errors / 0 warnings** |
| Ring validation (`Scripts/ValidateGuangzhouLandmarkShowcase.py`) | **0 errors / 0 warnings**, 8 landmarks, 0 overlaps |

Union footprint **5320 × 1080 × 1689.68 cm**, unchanged by the tuning. Triangle total
**206,252 → 218,328**, entirely from the finer arch. All 48 textures, 8 instances, 7 HISMs on a
`LevelInstanceComponent` root, Blueprint spawns to the union footprint, Zhengximen 0.000° off
the plaza.

### A broken API produced seven false failures

`EditorStaticMeshLibrary.get_num_uv_channels` returns **0 for every mesh** — including the
known-good Wenmingmen and Zhengnanmen meshes that render their 4K maps correctly. It reports
*source* UV channels, and an Interchange import carries no source data.
`StaticMesh.get_num_vertices` does not exist at all. **The check was deleted rather than kept**:
a check that fails good assets is worse than no check, and probing an API against a known-good
asset is what revealed it. The validator now reads the **source GLB's** vertex attributes and
asserts `TEXCOORD_0`, which is the check that would have caught the original defect.

## Files

**Added**

- `ContentSource/GuangzhouLandmarks/Zhengximen/source/generate_zhengximen_package.py` — the
  package's generator, staged into the project (the package's own copy stays in `Raw3DPacket/`)
- `ContentSource/GuangzhouLandmarks/Zhengximen/source/build_zhengximen.py` — rebuild driver
- `Scripts/RenderZhengximenPreviews.py` — Blender previews tuned to this footprint
- `Scripts/_ProbeZhengximenUVs.py` (transient diagnostic)

**Modified**

- `Scripts/ImportZhengximenLandmark.py` — no longer force-deletes a mesh before reimporting
  (the Blueprint's HISMs reference them)
- `Scripts/ValidateZhengximenLandmark.py` — UV check replaced; expected triangle total updated

**Generator changes** (all in the staged copy)

- `add_box(..., uv_unit=False)` — map a single non-repeating image across a face once
- `add_arched_leaf(...)` — new; a leaf whose top edge is the arc
- `scene_from_builder` / modular export — carry UVs instead of `face_colors`
- lower and upper timber walls — one continuous closed wall per side, ends included
- doors — arch-fitting leaves; arch ring 34 → 96 segments; wall slices 46 → 400 for LOD0;
  barrel joints moved flush

## Open items

- **Native visual capture is PENDING.** Everything above is structural and source-side
  evidence. A render needs an open editor with a live RHI, which a commandlet cannot provide,
  and UE Python remote execution cannot discover the running editor on this machine (multicast
  to `239.0.0.1:6766` gets no answer even with the port exclusively held; `adb`, which Unreal
  itself spawns for Android device detection, also binds it). Run
  `Scripts/CaptureZhengximenNativeBlueprint.py` with the editor open.
- **The plaque spells 正西门 (simplified) where the reference board shows 正西門.** Left alone:
  regenerating textures needs a CJK font (`/usr/share/opentype/noto/...`, a Linux path), and
  the wording is a content decision rather than a defect.
- **The reference board shows the gate open** — a passage with people walking through — while
  this model has closed doors. The doors were kept closed and made arch-fitting, because the
  model already modelled a closed door with studs and knockers, and a rectangular door under a
  taller vault read as broken. Say the word and the leaves can be opened instead.
- The gate's UCX collision and UV1 lightmap atlas are still absent: the package's LOD0 FBX does
  not import in this engine build (see the import record).
