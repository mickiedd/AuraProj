# Guangzhou landmark showcase level

## Intent

Gather every placeable landmark in `Content/Assets/Environment/GuangzhouLandmarks`
into one new level, arranged as a showcase. Two landmarks in that folder existed
only as bare `StaticMesh` assets with loose materials, so they could not be placed
the way the other seven are; those two were wrapped into building Blueprints first.

## Changed behaviour

### New Blueprint Actor Assets

Both follow the convention already used by `BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset`
and established by `Scripts/CreateGreatSouthGateActorAsset.py`: a `PackedLevelActor`
Blueprint holding one `HierarchicalInstancedStaticMeshComponent` per source mesh,
with the source transform recorded as an instance transform.

| Asset | Source mesh | Footprint (cm) | Height (cm) |
| --- | --- | --- | --- |
| `GreatNorthGate/BP_GreatNorthGate` | `GreatNorthGate/SM_GreatNorthGate` | 3600 × 1750 | 2476 |
| `ZhenhaiTower/BP_ZhenhaiTower` | `ZhenhaiTower/SM_ZhenhaiTower` | 3466.6 × 2834.6 | 2085 |

Both are single meshes sitting at identity, so each gets one HISM component with one
instance. Wrapper footprints match their source meshes to 0.0 cm. Each wrapper is
verified by spawning it and comparing bounds to the source mesh rather than by
trusting `compile_blueprint`'s return value (see the trap below).

The legacy `Xiaobeimen/SM_Xiaobeimen` and `SM_Xiaobeimen_GeometryFixed` meshes were
deliberately **not** wrapped: that gate already has two V3 Blueprints, so a wrapper
would be a third redundant Xiaobeimen.

### New level

`Content/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase`

- Layout: a ring around a central plaza, each landmark turned so its front faces the
  centre. `yaw = ring angle − 90°`, which turns local −Y (the front convention the
  existing Scifi Desert landmark placement relies on) back toward the origin.
- Ring radius **10612.12 cm**, derived from the landmarks' own geometry rather than
  hard-coded: each contributes its bounding-circle diameter plus a 3000 cm gap of
  arc, and the radius is that total arc over 2π. Swapping in a larger or smaller
  variant keeps the ring valid.
- Ground plane: `/Engine/BasicShapes/Plane` scaled to 420 × 420 m, top surface at
  `Z = 0`, using a new `M_ShowcaseGround` material.
- Lighting: a 7.0 key plus a 2.0 unshadowed fill directional light, a 1.4 sky light
  with a black lower hemisphere, sky atmosphere, and an unbound post-process volume
  pinning exposure at EV100 3.0.
- 9 text labels naming each landmark, and a `PlayerStart` at the plaza centre.
- 25 actors total: 9 landmarks, 1 ground plane, 2 directional lights, 1 sky light,
  1 sky atmosphere, 1 post-process volume, 9 text labels, 1 player start.

| # | Landmark | Ring angle | Yaw | Components / instances |
| --- | --- | --- | --- | --- |
| 1 | Zhengnanmen (Great South Gate) — HighFidelity | 8.613° | −81.387° | 114 / 12383 |
| 2 | Zhengnanmen (Great South Gate) — AAA V3 | 41.545° | −48.455° | 19 / 19 |
| 3 | Xiaobeimen (Small North Gate) — AAA V3 | 81.262° | −9.536° | 46 / 46 |
| 4 | Xiaobeimen (Small North Gate) — Production V3 | 125.994° | 35.936° | 10 / 10 |
| 5 | Guidemen (Guide Gate) — V5 4K | 173.050° | 83.050° | 47 / 18822 |
| 6 | Wuxianmen (Five Immortals Gate) — V5 4K Core | 215.547° | 125.557° | 7 / 5103 |
| 7 | Wuxianmen (Five Immortals Gate) — V5 FullPBR | 251.879° | 161.890° | 7 / 5103 |
| 8 | GreatNorthGate (Dabeimen) | 289.601° | 199.601° | 1 / 1 |
| 9 | Zhenhai Tower | 330.523° | 240.523° | 1 / 1 |

Component and instance counts for landmarks 1–7 match the figures already recorded
for those assets in the project's own audit notes, which confirms the placement did
not alter the assets.

## Traps found

**1. `unreal.Rotator`'s positional order is `(roll, pitch, yaw)`, not
`(pitch, yaw, roll)`.** `unreal.Rotator(0.0, yaw, 0.0)` therefore sets **pitch**. The
first build of this level produced a ring of landmarks tipped over on their sides,
and `get_actor_bounds` reported Guidemen as 5730 cm tall. Confirmed by construction:
`Rotator(11, 22, 33)` reads back `pitch=22, yaw=33, roll=11`, while
`Rotator(pitch=11, yaw=22, roll=33)` reads back as written. Rotators in the new
scripts are built with keyword arguments, and each placement now asserts that pitch
and roll are 0 after the yaw is applied.

**2. Hidden collision volumes must not ground a model.** Every one of the nine
landmarks carries hidden UCX collision components whose lowest point sits exactly
**128 cm below the actor origin**. `get_actor_bounds` includes them, so it reports
every correctly grounded landmark as sunk by about a metre. Spacing and grounding are
instead computed from the actual instanced mesh data — component relative transforms
and instance transforms folded in, non-rendering components excluded.

**3. `unreal.BlueprintEditorLibrary.compile_blueprint` returns `None` in this build**
(void, not `bool`), so asserting on its return value fails even on success. The
wrappers are verified by spawning them instead.

This trap had a consequence worth recording. The assert aborted the first run *after*
`create_blueprint_asset_with_parent` had made the asset but *before* `save_asset`, so
`BP_GreatNorthGate` existed only in the editor's memory: the asset registry resolved it,
the level referenced it, and spawning it worked — but there was no `.uasset` on disk, and
it would have been lost on the next editor restart. `WrapLandmarkMeshBlueprints.py` now
saves on its reuse path as well, and asserts that the `.uasset` actually exists on disk
before reporting success. Both wrappers are confirmed written (32,723 and 32,813 bytes).

## Validation

`Scripts/ValidateGuangzhouLandmarkShowcase.py`, run against the saved level reloaded
from disk (nothing validated from in-memory state) — **0 errors, 0 warnings**:

- all 9 landmarks present exactly once; no unexpected landmark-tagged actors
- actor Z matches the placement manifest to within **0.004 cm** on every landmark
- **0** XY overlaps between landmarks
- every landmark inside the 420 m ground plane
- ring radii 106.12–115.94 m, angular gaps 32.9°–47.1°, full 360° coverage
- supporting actors present: 2 directional lights, 1 sky light, sky atmosphere,
  1 player start, 9 text labels, 1 ground plane

Manifests: `Saved/RawModelImport/guangzhou-landmark-showcase.json`,
`Saved/RawModelImport/guangzhou-landmark-showcase-validation.json`,
`Saved/RawModelImport/landmark-wrapper-blueprints.json`.

## Renders

`Scripts/CaptureGuangzhouLandmarkShowcase.py` captures top-down, oblique and
plaza-level views to `Saved/RawModelImport/GuangzhouLandmarkShowcase/`. The top-down
and oblique views confirm the ring layout: nine landmarks evenly spread around a
central plaza, all sitting on the ground plane.

### Exposure, and a wrong diagnosis corrected

An earlier pass of this work concluded the level was rendering **unlit**, because the
ground appeared to draw at exactly its own base colour and hiding every light —
including the key — left the image byte-identical. **That conclusion was wrong, on two
counts:**

1. **Hiding a light actor does not disable the light.** `set_actor_hidden_in_game` does
   not stop a `LightComponent` contributing, so the "hide the lights" test proves
   nothing and returns an identical image whether the scene is lit or not.
2. The scene is in fact **lit but severely over-exposed**, which flattens it: bright
   surfaces clip to pure white and dark ones to black, leaving almost nothing in
   between. That is what produced the bimodal histograms.

Confirmed by bracketing exposure with a temporary unbound post-process volume and
looking at the results — `Saved/RawModelImport/GuangzhouLandmarkShowcase/ev-{20,30,40,50}.png`
show the Zhengnanmen gate rendering correctly, with pale stone and its coursing, the
dark arch opening, the red beam rows, the gold ridge ornaments and the roof, against a
blue sky. At EV100 3.0 the gate is legible but the stone still clips; at 4.0 the stone
reads correctly but the frame goes dark.

Two further facts rule the level out as the cause:

- **The project's own preview level behaves identically.** Capturing
  `L_Xiaobeimen_AAA_V3_Preview` with the same setup renders the same washed-out frame
  (`ab-preview.png`, `tight-preview.png`), and framing the *same asset* in both levels
  gives comparable histograms (`tight-preview` 4.1% dark / 4.6% mid / 85.9% bright vs
  `tight-showcase` 9.0% / 0.9% / 89.3%). The showcase lighting recipe (key 7.0, fill
  2.0, sky 1.4, sky atmosphere) closely matches the preview levels' (8.0 / 2.0 / 1.4 /
  atmosphere).
- **The harness is not reproducible.** Identical settings give different renders
  between runs, and the level's own unbound post-process volume does not reproduce the
  value found by bracketing, while a runtime volume at higher priority does. The
  preview levels' own exposure pin (EV100 = 1.0) is simply too bright for these scenes.

**Resolution:** the level pins exposure at EV100 3.0 (its own unbound volume) rather
than copying the preview levels' 1.0, and `CaptureGuangzhouLandmarkShowcase.py` pins the
same value for its own captures so they are deterministic. Layout is confirmed
visually; lighting is confirmed correct by the bracket images but should still be
eyeballed once in the editor, since single-frame captures cannot reproduce the
converged auto-exposure or the temporal sky sampling a live viewport has.

## Files

New scripts:

- `Scripts/WrapLandmarkMeshBlueprints.py`
- `Scripts/CreateGuangzhouLandmarkShowcase.py`
- `Scripts/CreateShowcaseGroundMaterial.py`
- `Scripts/ValidateGuangzhouLandmarkShowcase.py`
- `Scripts/CaptureGuangzhouLandmarkShowcase.py`
- `Scripts/InspectLandmarkWrappers.py`, `Scripts/InspectLandmarkBlueprintDetail.py`
- `Scripts/ProbeShowcaseLevelPrereqs.py`, `Scripts/ProbeShowcaseLandmarkBounds.py`,
  `Scripts/ProbeShowcaseRaycastGrounding.py`, `Scripts/ProbeShowcaseRotation.py`,
  `Scripts/ProbeRotatorOrder.py`, `Scripts/ProbeV3LandmarkComponents.py`,
  `Scripts/ProbeShowcaseGroundMaterial.py`, `Scripts/ProbeShowcaseGroundAssignment.py`,
  `Scripts/ProbeShowcaseLightingReference.py`, `Scripts/ProbeShowcaseRenderTargetRange.py`
- `Scripts/DiagnoseLandmarkWrappers.py`, `Scripts/DiagnoseShowcaseGroundBrightness.py`
- `Scripts/BracketShowcaseExposure.py`, `Scripts/BracketShowcaseSkyLight.py`,
  `Scripts/TestShowcaseGroundMaterials.py`, `Scripts/IsolateShowcaseGroundVeil.py`

New content:

- `Content/Assets/Environment/GuangzhouLandmarks/GreatNorthGate/BP_GreatNorthGate`
- `Content/Assets/Environment/GuangzhouLandmarks/ZhenhaiTower/BP_ZhenhaiTower`
- `Content/Assets/Environment/GuangzhouLandmarks/M_ShowcaseGround`
- `Content/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase`
- `Content/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture` (capture target)

[Visual summary diagram](2026-09-21-guangzhou-landmark-showcase-level.svg)
