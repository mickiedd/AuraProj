# Great West Gate AAA material regeneration — 2026-09-15

## Intent

Regenerate the Great West Gate (Zhengximen) materials manually to a higher-fidelity PBR presentation while preserving the authored mesh assembly and its independent Blueprint.

## Changed behavior

- Added eight rollback-safe `M_*_AAARedone` materials under `/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Materials/`.
- Rebuilt each graph from the authored six-map 4K set: BaseColor, Normal, Roughness, Metallic, AO, and Height.
- Restored albedo readability with controlled lift and palette blending instead of multiplying the entire source albedo by a mid-gray tint.
- Added coherent height-driven BumpOffset microdetail, retained Nanite compatibility, and preserved two-sided rendering required by the source-facing assembly.
- Rebound the primary mesh slots and both `BP_Zhengximen_V4` static-mesh component templates to the new materials.
- Left the previous `_AAA` and `_ReferenceTuned` assets in place for rollback. Geometry, UVs, collision, and placement were not changed.

## Validation

- `python -m py_compile Scripts/RegenerateZhengximenAAAMaterials.py Scripts/ValidateZhengximenAAAMaterials.py` — passed.
- `python Scripts/remote_run.py Scripts/ValidateZhengximenAAAMaterials.py` — passed in the live editor; seven slots, seven six-map materials, UV0, complex-as-simple collision, and two Blueprint components verified.
- `UnrealEditor-Cmd.exe Aura.uproject -run=pythonscript -script=Scripts/ValidateZhengximenAAAMaterials.py -unattended -NullRHI -nosound -nosplash -nop4` — exit code 0 after the validator’s commandlet-safe manifest path; UV verification is explicitly live-editor-only because `StaticMeshEditorSubsystem` is not exposed in commandlets.
- `capture_3_front.py`, `capture_3_close.py`, and `capture_3_rear.py` — completed after the saved rebind; all three PNGs were visually inspected.
- The shared V4 validator remains a broader workspace check and still has unrelated pre-existing assumptions about other gates’ `_AAA` bindings and Dadongmen height-map dimensions; the focused Zhengximen validator is the authoritative check for this pass.

## Evidence

- Regeneration manifest: `Saved/RawModelImport/V4/Zhengximen-AAA-material-regeneration.json`.
- Focused validation report: `Saved/RawModelImport/V4/Zhengximen-AAA-validation.json`.
- Visual captures: `Saved/RawModelImport/V4/Zhengximen_V4-front.png`, `Zhengximen_V4-close.png`, and `Zhengximen_V4-rear.png`.
- [Visual summary diagram](2026-09-15-great-west-gate-aaa-material-regeneration.svg).
