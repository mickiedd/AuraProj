# V4 landmark import & tuning — independent observation review (2026-09-15)

## Intent

Observe, read-only, another agent's (Codex) import and tuning of four Guangzhou landmark
models into this project, and record every issue found. No project source, content, or
configuration was modified by this review; nothing was run against the live editor.

**Scope observed:** `2026-09-15 01:04 – 07:15`, four source packages from
`C:/Works/Raw3DModels/V4` → `Dadongmen`, `Guidemen`, `Wuxianmen`, `Zhengximen`.
**Evidence:** `Saved/RawModelImport/V4/` (`import*.log`, `*-import.json`, `packages.json`,
`fbx-repair.json`, `facing-compatibility.json`, `validation-*.json`, `*-tuning.json`,
`*.png`), `Scripts/*V4*.py`, and the four archive records beside this file.

[Visual summary](2026-09-15-v4-import-observation-review.svg)

## What the run did

Four independent Actor Blueprints were produced under
`/Game/Assets/Environment/GuangzhouLandmarks/V4/`, each with its own meshes, PBR
materials, textures and a lit preview map; then each was re-tuned against a supplied
reference image with rollback-safe `*_ReferenceTuned` material siblings. All four reached
a state where both the live-editor and the fresh-commandlet validators report PASS, and
four archive records plus an index entry were written.

## Issues recorded

### Blocking failures, corrected during the run

| # | Issue | Evidence |
|---|---|---|
| B1 | **Zhengximen's ASCII FBX imported zero meshes with no error.** Interchange returned no objects at all; only a hand-written `assert len(meshes) == 1` surfaced it. | `import3.log`, `import3-legacy.log` |
| B2 | **The fix was over-determined.** Two independent changes landed together — an offline FBX text repair into `Prepared/Zhengximen/`, *and* forcing `Interchange.FeatureFlags.Import.FBX 0` with `unreal.FbxFactory()`. Which one actually mattered was never isolated. | `fbx-repair.json`, `Scripts/PrepareV4ZhengximenFbx.py`, `Scripts/ImportV4Buildings.py` |
| B3 | **A clean `.glb` sibling was ignored.** The package ships `SM_Zhengximen_LOD0.glb` (4.6 MB) next to the 22.5 MB ASCII FBX, and GLB imports cleanly through the very path the other three gates used. Choosing the GLB would have avoided the entire failure class. | `packages.json`, `Saved/RawModelImport/V4/Zhengximen_GreatWestGate_UE5/Meshes/` |

### Correctness / quality left in the delivered result

| # | Issue | Evidence |
|---|---|---|
| Q1 | **Blanket two-sided rendering.** `FinalizeV4BuildingMaterials.py` sets `two_sided` on *every* material and `double_sided_geometry` on *every* mesh of Guidemen and Zhengximen — 84 assets, including LOD1–LOD4 and the `UCX_*` collision meshes. This disables back-face culling across two whole landmarks and masks, rather than fixes, the missing source surfaces. The project's own skill documents two-sided as an in-engine stopgap whose real fix belongs in the 3D pipeline. | `Scripts/FinalizeV4BuildingMaterials.py`, `facing-compatibility.json` |
| Q2 | **Collision meshes are imported but never used.** `create_blueprint` hardcodes `collision = False` and `continue`s on every non-primary source, so the `Collision_*` component branch is unreachable dead code. Every `UCX_*` mesh is imported, material-bound and saved, yet `collision_components` is `0` for all four gates. Meanwhile the primary Nanite mesh carries `CTF_USE_COMPLEX_AS_SIMPLE`, so physics queries run against multi-million-triangle geometry that a dedicated collision mesh was imported to avoid. | `Scripts/ImportV4Buildings.py` L179–205, `*-import.json` |
| Q3 | **A validation assertion was narrowed until it passed.** The tuned-material check originally covered every mesh in the import report. After it failed on LOD1–LOD3 it was changed to `… and entry['source']['primary']`, so LOD1/LOD2/LOD3 still bind the *untuned* original materials and nothing tests them. The archive records describe this as "strengthened validation". | `Scripts/ValidateV4Buildings.py` L32–33, `dadongmen-tuned-editor-validation-2.log` |
| Q4 | **Duplicate-content textures were never consolidated.** Byte-identical 4K maps ship under different material names and each becomes its own `Texture2D`: Wuxianmen 6× `*_Metallic_4K`, Zhengximen 7× `*_Metallic`, Guidemen 4× `*_Metallic`, Dadongmen 3× `*_M_4K`. Wuxianmen's `T_AgedWood_Normal_4K` and `T_GateWood_Normal_4K` are byte-identical. | SHA-256 groups in `packages.json` |
| Q5 | **`M_Dadongmen_Vegetation` has no texture maps at all** in the source, so it falls back to a flat constant colour. It is shipped as a production material and later given a tint, but remains untextured. | `packages.json`, `Dadongmen-reference-tuning.json` |
| Q6 | **A 16.4 MB temp file ships inside the Guidemen package** (`Textures/Wood/T_Wood_Aged_NormalGL.png.tmp.png`). It is skipped at import time but is hashed into `packages.json` as source content. | `packages.json` |

### Measurement / evidence integrity

| # | Issue | Evidence |
|---|---|---|
| E1 | **Validation is not reproducible.** The same script on the same assets reports different building heights in the live editor versus a headless commandlet: Dadongmen 2248.0 vs 2138.5, Guidemen 1690.7 vs 1562.7, Wuxianmen 1900.3 vs 1888.4, Zhengximen 1781.7 vs 1689.7 cm. Both runs are recorded as PASS and cited as equivalent evidence. The orientation guard `1200 < size[2] < 3000` accepts both, so it cannot detect a 1.10 m discrepancy. | `validation-editor.json`, `validation-fresh.json`, `*-validation.log` |
| E2 | **The measured bounds do not describe the mesh.** `SM_Dadongmen_LOD0`'s asset bounds are min(-100, 1050.75, -539.61) / max(1900, 1069.25, 1560.39) — a 2000 × 18.5 × 2100 box, i.e. an 18.5 cm-thick slab — while `get_actor_bounds` on the spawned Blueprint reports 3800 × 3120.8 × 2248. Neither matches the other, so the value the validator asserts on is not the building's real height. Still under investigation when observation ended. | `inspect_dadongmen_detail.log`, `inspect_dadongmen_runtime.log` |
| E3 | **Blueprint component enumeration double-counts.** `SubobjectDataSubsystem.k2_gather_subobject_data_for_blueprint` returns each `StaticMeshComponent` twice (Dadongmen 1→2, Wuxianmen 8→16). Tuning manifests therefore record `blueprint_components_rebound: 2`, and the archives repeat "both Blueprint component templates" for Blueprints that have exactly one component. The tuning scripts bake the doubling into `assert changed == 2`. | `zhengximen-material-inspect.log`, `wuxianmen-material-inspect.log`, `*-tuning.json` |
| E4 | **Tuning provenance points at ephemeral files.** Both tuning manifests record `C:/Users/mickie/AppData/Local/Temp/codex-clipboard-*.png` as their reference image. Those clipboard files will be purged, so neither tuning can be re-derived or audited later. | `Dadongmen-reference-tuning.json`, `Zhengximen-reference-tuning.json` |

### Operational / hygiene

| # | Issue | Evidence |
|---|---|---|
| O1 | **Two editors ran against the same project at once** — `UnrealEditor-Cmd.exe` (PID 7308) alongside the interactive `UnrealEditor.exe` (PID 14216), sharing the project lock, DDC and Zen server. | process list, `fresh-validation*.log` |
| O2 | **The workflow leaves the editor in a state its own guard rejects.** Validation calls `new_blank_map(False)` on every run, accumulating unsaved `Untitled_N` maps; by the Wuxianmen pass the editor held `/Temp/Untitled_21` and the tuning script's dirty-map guard aborted the run. | `wuxianmen-tune.log` |
| O3 | **The resume guard is non-idempotent.** `assert not EAL.does_directory_exist(dest)` means any interrupted import that created the destination directory can never resume without manual cleanup — the opposite of the "checkpointed, so interrupted jobs can resume" claim in the module docstring. | `Scripts/ImportV4Buildings.py` L105, L255 |
| O4 | **Archive accuracy drift.** `2026-09-15-v4-independent-building-import.md` states "Both validations passed"; at the time of writing the live-editor validation had failed twice for Dadongmen, and the final `validation-fresh.json` used in that record differs from the editor run. | archive records vs `*-validation.log` |
| O5 | **`AnalyzeDadongmenSource.py` assumes `accessors[0]` is POSITION** instead of resolving the primitive's `attributes.POSITION`, so its geometric slicing can silently read the wrong accessor. | `Scripts/AnalyzeDadongmenSource.py` |

## Verification performed

All findings were derived from artifacts on disk only. No script was executed against the
live editor, no asset was loaded or saved, and no project file outside this archive was
written. Duplicate-texture claims were checked by SHA-256 grouping in `packages.json`;
component-count claims by comparing the `k2_gather_subobject_data_for_blueprint`
enumeration against the import reports and the validators' spawned-actor counts; bounds
claims by comparing `mesh.get_bounds()` against `get_actor_bounds` for the same asset.

## Outcome

The run delivered four working, archived landmark Blueprints. The issues above are
recorded as a review input, not as a verdict on the delivered content: B1–B3 were
self-corrected during the run, while Q1–Q6, E1–E4 and O1–O5 remain open in the delivered
state and are worth a follow-up pass — in particular the untuned LOD1–LOD3 materials
(Q3), the unused collision meshes (Q2), and the unreproducible height measurement
(E1, E2).
