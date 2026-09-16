# Zhengnanmen Great South Gate reference tuning

## Intent

Tune the live `Zhengnanmen_AAA_V3` Great South Gate toward the supplied
reference sheet while keeping its authored three-level silhouette, signboard,
arched door, roof ornaments, moss, and collision topology.

## Changed behavior

- Added twelve reusable Blueprint components named `ReferenceInfill_L{0..2}_{F,B,L,R}`.
- The panels use the package-local `M_DoorWood` material and sit just behind the
  red lattice on the front, rear, left, and right faces at all three timber
  levels. They remove the bright preview-background gaps and restore the dark
  timber reading shown in the reference.
- Updated the V3 reports and placement manifest to record 19 visible components
  and 6 hidden collision components. The six authored collision components were
  left unchanged.
- Updated the broad V3 validator so authored reference infill and the earlier
  corrected Production roof/door meshes are validated separately from source
  component coverage.

## Validation

- `python -m py_compile Scripts/TuneZhengnanmenReference.py Scripts/ValidateV3Buildings.py` — passed.
- Unreal remote `AuditV3BuildingDetails.py` — passed; 25 total components for
  Zhengnanmen, 19 visible and 6 collision.
- Unreal remote `ValidateAllV3Details.py` — passed for all three V3 buildings.
- Unreal remote `ValidateV3BuildingPlacement.py` — passed; three grounded,
  non-overlapping showcase actors and 1,586 existing geometry actors checked.
- Unreal remote `ValidateV3Buildings.py` — passed in a fresh blank validation
  world for all three Blueprints.
- Front and rear visual captures were rendered after Blueprint reload; the
  panels remain behind the lattice and leave the signboard and arched door
  unobstructed.

## Illustration

[Reference tuning flow](2026-09-14-zhengnanmen-reference-tuning.svg)

