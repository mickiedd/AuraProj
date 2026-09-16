# V3 building package import — 2026-09-14

Import all three packages from `C:\Works\Raw3DModels\V3` as new, independent, intact building Actor Blueprints. No existing building is replaced.

[Archived visual summary](2026-09-14-v3-building-import.svg)

## Result

Content root: `/Game/Assets/Environment/GuangzhouLandmarks/V3`.

| Blueprint | Mesh sources / assets | Textures | Visible / collision components |
|---|---:|---:|---:|
| `BP_Xiaobeimen_AAA_V3` | 21 / 237 | 56 | 38 / 5 |
| `BP_Xiaobeimen_Production_V3` | 7 / 62 | 90 | 10 / 6 |
| `BP_Zhengnanmen_AAA_V3` | 12 / 49 | 43 | 7 / 6 |

Each Blueprint is a native Actor with a complete source high-detail assembly, local materials/textures and hidden, blocking components from its supplied simple-collision model. It does not reference a preview level or another building package. Each can be dragged into another level as a single building. Nanite is enabled on the primary high-detail meshes.

All 40 canonical GLTF/GLB sources were imported, including full variants, LODs, modular parts and collision meshes. All 189 files under package Textures directories were imported, including full-resolution maps, proxies and optional height/ORM/calligraphy maps. Alternate OBJ/STL/USD encodings, original previews, generators and documentation remain intact in the source packages and are inventoried in the validation packet; they are not redundantly layered into the visible building. Alternate LOD assets are available separately; automatic runtime LOD chains were not assembled.

## Repairs and behavior

- The supplied `Textures/Moss/Moss_ORM_4K.png` is truncated mid-IDAT. Regenerated a 4096×4096 RGB texture manually from intact AO, Roughness and Metallic maps. R/G/B equality is verified pixel-for-pixel. Imported as `Moss_ORM_4K_Rebuilt`; original archive and extracted file remain unchanged.
- South Gate GLBs have no UVs. Derived import copies add per-face projected UVs and a unique signboard projection. All original triangle positions, counts, nodes, materials and assembly transforms are preserved exactly.
- Corrected Unreal Python Rotator positional-order ambiguity by explicitly naming pitch, yaw and roll on Blueprint components. Final transforms match imported source transforms.
- Preview lighting and ground are confined to the new preview maps. They are not Blueprint runtime dependencies.

## Validation

- `python Scripts/PrepareV3Buildings.py`: PASS; source inventory, derived import files, baseline hashes.
- `python Scripts/ValidateV3BuildingSources.py`: PASS; 1161 original source files unchanged; 234 PNGs structurally checked and decoded (the one damaged source is represented by its regenerated import copy); 11 UV-derived GLBs preserve every triangle position; 419 existing landmark assets and project maps unchanged.
- `python Scripts/remote_run.py Scripts/ValidateV3Buildings.py`: PASS in the live editor; 3/3 independent spawns, exact component/mesh coverage, transform comparisons, collision boxes, material assignments, texture dimensions/color space, and dependency closure.
- Fresh process: `C:\Git\UnrealEngine-5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Git\AuraProj\Aura.uproject -run=pythonscript -script=C:\Git\AuraProj\Scripts\ValidateV3Buildings.py -unattended -nullrhi -nosplash -nosound -NoSourceControl`: PASS, exit 0. Evidence: `Saved/RawModelImport/V3/fresh-editor-validation-pass.log`.
- Front and rear screenshots of all three final Blueprint instances inspected. Package-authored proportions, stylized materials and roof details retained; this import does not claim a historical or artistic reconstruction beyond the supplied meshes.
- No packaged-game performance or player-navigation test was requested/run. Supplied simple collision is present; game-specific collision/navigation tuning remains outside this import.
- Source tangent/degenerate-UV warnings on some original meshes remain documented in import logs; completed assemblies were visually inspected. No material compilation failures in the final validation log.

## Reproduction and review

Relevant scripts: `PrepareV3Buildings.py`, `ImportV3Buildings.py`, `FinalizeV3Buildings.py`, `ValidateV3BuildingSources.py`, `ValidateV3Buildings.py`, `CaptureV3Buildings.py`, and existing `remote_run.py`, all under `Scripts/`.

Per-source import checkpoints and validation reports: `Saved/RawModelImport/V3/`. The importer rejects uncheckpointed partial destinations and never replaces an existing building. Complete packages are reused on rerun. Source packages must be available at the recorded local paths for a full reimport.

The local implementation validation packet, complete binary inventory with hashes, reports, scripts and six screenshots are at `C:/Users/mickie/.codex/visualizations/2026/09/13/01a09b9b-b990-7450-b5f6-d96b77969704/V3-building-import`. The current handoff skill prepares a local packet; no external reviewer or service was contacted. Fresh-process validation is local verification, not independent human/agent review.
