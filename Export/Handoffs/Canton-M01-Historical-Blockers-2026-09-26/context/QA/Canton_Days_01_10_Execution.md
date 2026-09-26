# Canton days 01–10 native continuation — 2026-09-26

Baseline `e3f8e2c9b74b5f3c782c3111857ae4d53ab37b7b`, Codex, UE 5.5.4 CL 40574608, Mac/Metal (SM5 reported by automation), Python 3.9.6. Working source revisions are identified by the existing source manifest, provisional export metadata and native snapshot. Unrelated landmark changes were preserved.

Run from `/Volumes/M2/Works/AuraProj`:

```sh
/Volumes/M2/Engine/UE_5.5/Engine/Build/BatchFiles/Mac/Build.sh AuraEditor Mac Development /Volumes/M2/Works/AuraProj/Aura.uproject -WaitMutex
Saved/TerrainTools/venv/bin/python Scripts/prepare_canton_ue_import.py
Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py
zsh Scripts/run_canton_provisional_review.sh import
zsh Scripts/run_canton_provisional_review.sh review
zsh Scripts/run_canton_provisional_review.sh automation
```

Build and successful editor runs exited 0. `import` is for an absent map only and refuses to overwrite an existing Landscape. The successful initial import and first reload used the equivalent direct editor arguments now retained in the wrapper. The final exposure-adjusted capture and automation use the wrapper. `review` and `automation` load existing packages without saving map changes. Generated validation JSON is removed before each wrapper run so a stale pass cannot mask a failed run.

Engine logs: `Saved/Logs/CantonImport.log`, `Saved/Logs/CantonReview.log`, `Saved/Logs/CantonAutomation.log`. The compact automation report and native results are archived in `QA/UE_Import_Screenshots/`; the full generated web report is in `Saved/Reports/CantonAutomation/`.

Failures corrected during implementation:

- The Landscape edit API requires the Foliage module header dependency; the editor module now declares it.
- macOS defines `Size`; the local constant was renamed `VertexCount`.
- Loading a just-created inactive factory world caused an Unreal world-leak assertion. Map creation now uses `GEditor->NewMap(true)` and saves that editor world directly. The failed empty map was moved to ignored `Saved/TerrainTools/FailedEmptyWorld.umap`; the subsequent creation, reload and automation succeeded.
- The source overlay is RFC 7946 longitude/latitude. Preparation explicitly converts it to EPSG:32649; the native placement script consumes this converted file, with inverse-conversion tests.
- The first screenshots were too dark for visual review. Native capture exposure was increased; terrain and map geometry were not altered for the captures.

The build still reports the existing Aura/AuraAbilityGraph circular-reference warning. Editor startup logs contain pre-existing menu registration and locale warnings/errors outside the test session. The focused automation report has zero test warnings/errors; no whole-project clean-log claim is made.

Screenshots were visually inspected for continuous coverage, northern relief and wall/gate overlay legibility. The source remains a modern DTM, so its residual surface texture/modern alteration is not interpreted as nineteenth-century terrain. Collision tests validate encoded samples, not the unknown historic ground surface.

Historical M01 remains blocked. See `Review/M01_Terrain_Review.md` for the separate successful provisional technical result.

## Final snapshot and integrity checks

```sh
python3 Scripts/refresh_canton_terrain_manifest.py
python3 Scripts/refresh_canton_m01_provisional_manifest.py
python3 Scripts/test_canton_source_manifest.py
Saved/TerrainTools/venv/bin/python Scripts/test_canton_heightmap_contract.py
Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py
python3 Scripts/refresh_canton_native_manifest.py
python3 Scripts/test_canton_m01_readiness.py
git diff --check
```

All exit 0. Source snapshot: 46 artifacts; earlier provisional snapshot: 21; native snapshot: 472 including external actor packages. Python emits the existing affine matrix-multiplication deprecation warning. The new SVG archive was rendered with `rsvg-convert` and visually inspected. Relative documentation links and native artifact inclusion were checked.
