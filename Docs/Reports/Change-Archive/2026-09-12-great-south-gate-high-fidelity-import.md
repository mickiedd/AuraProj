# Great South Gate Zhengnanmen high-fidelity scene import

## Intent

Import the supplied `GreatSouthGate_Zhengnanmen_UE5_Complete_Package.zip` into
AuraProj without overwriting the existing South Gate asset or mutating the
showcase landmark row. The package path resolves on disk as
`C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package.zip`;
the underscore-separated directory path in the request was not present.

## Changed behavior

- Imported `GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb` with UE5.5
  Interchange glTF scene import.
- Kept repeated geometry shared with `bake_meshes = False` and
  `combine_static_meshes = False`; enabled Nanite on imported static meshes,
  disabled automatic collision generation, and applied the established
  `import_offset_rotation = roll -90 degrees` for the Z-up source.
- Created the isolated content folder
  `/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity`
  containing 112 shared static mesh assets, 7 imported materials, and 17
  imported textures, plus the preview map
  `GreatSouthGate_Zhengnanmen_HighFidelity_Preview`.
- Created 12,195 scene mesh actors in that preview map. The imported scene
  measures `2700.000 x 1705.353 x 2303.482 cm`, is grounded at `Z = 0`, and
  retains the source's 179,312 stored triangles while reusing the mesh assets
  across the 12,195 node placements.
- Left the existing
  `GuangzhouLandmark_GreatSouthGate` actor, its previous HighPoly asset, and
  `L_showcase_level` untouched. The new preview level is ready for inspection
  or for a later explicit replacement/placement decision.

## Validation

- `python Scripts/AnalyzeDabeiMenGlb.py C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package/GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb Saved/RawModelImport/GreatSouthGate_Zhengnanmen_HighFidelity-source-analysis.json` — passed; 112 meshes, 12,196 nodes, 7 materials, 17 textures, and 179,312 source triangles.
- `$env:UE_ENGINE_ROOT='C:/Git/UE_5.5'; python Scripts/remote_run.py Scripts/ImportGreatSouthGateZhengnanmenHighFidelity.py` — passed; imported and saved the 112 shared Nanite meshes and preview level.
- `$env:UE_ENGINE_ROOT='C:/Git/UE_5.5'; python Scripts/remote_run.py Scripts/ValidateGreatSouthGateZhengnanmenHighFidelity.py` — passed; 112 static meshes, 12,195 static-mesh actors, expected bounds, Nanite enabled, assigned materials, and no validation errors.
- `$env:UE_ENGINE_ROOT='C:/Git/UE_5.5'; python Scripts/remote_run.py Scripts/CaptureGreatSouthGateZhengnanmenHighFidelityVisual.py` — passed; the rendered preview visibly shows the three-level roof, stone body, timber structure, and sign materials.
- [Rendered preview capture](../../Saved/RawModelImport/great_south_gate_zhengnanmen_high_fidelity_preview.png)
- [Visual summary diagram](2026-09-12-great-south-gate-high-fidelity-import.svg)

## Known limits and follow-up

- Collision is deliberately not generated for this decorative Nanite scene;
  author a lightweight gameplay collision solution separately if the gate must
  block or receive traces.
- The package's optional 4K source maps remain outside the project; the GLB's
  embedded practical PBR maps are the imported textures.
- This job does not replace or reposition the existing South Gate in the
  showcase map. That should be a separate explicit layout decision because the
  new preview contains 12,195 imported scene actors.
