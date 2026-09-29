# Canton district continuation — 2026-09-29

This candidate continues the existing provisional district. It does not replace the source frame or promote modern elevations to historical evidence. The owner approved the M4 Mac/Metal raster target, closed Wenmingmen and the declared performance budget on 2026-09-29.

Reproduce in order:

1. `Saved/TerrainTools/venv/bin/python Scripts/generate_canton_district_assets.py` generates project-authored geometry and procedural textures; no externally licensed art is embedded.
2. Build the editor with `./BuildEditor.command`. Run `Scripts/CompleteCantonDistrict.py` using UE's `-ExecutePythonScript`, then `Scripts/FinalizeCantonDistrict.py`. Both operate only on the district; preserve the locked imported terrain. The finalizer checks material graph connections, assigns material instances and physical materials, grounds the raised approach, disables the original gate's instance collision, and builds navigation with simple replacement proxies.
3. Run `python3 Scripts/run_canton_district_check.py traversal` and `review` for fresh navigation, physics-floor, map-check and screenshot evidence.
4. Build `Aura Mac Development`. `Scripts/repair_canton_mac_framework.py` repairs only the generated app's CEF framework layout if Xcode rejects its flat layout. Run `python3 Scripts/cook_canton_delivery.py` for the explicitly scoped two-map cook; it must leave DefaultGame.ini unchanged.
5. Run `python3 Scripts/stage_canton_delivery.py`, then run `Scripts/run_canton_runtime_check.py traversal --binary <game>` and `performance --binary <game>`. The harness rejects editor-binary evidence, missing routes, missing render counters and budget failures. Reconcile the raw sample CSV with `Scripts/check_canton_runtime_samples.py`.

Launch interactively using `./PlayCantonDistrict.command`. This profile uses `-nocef` because it has no browser UI. The runtime runner writes into the app’s macOS container and copies fresh evidence back to this directory, preserving sandboxing.

Controls: WASD movement, mouse look, R toggles localized damp geometry. The map-specific character uses a 35 cm radius / 180 cm tall capsule, a 20 cm step limit and a 35° floor-angle limit. Those settings and the 6 cm navigation-floor screen are explicit working engineering criteria pending owner review. Wenmingmen remains closed; the threshold route stops approximately 0.6 m outside the door.

The authored bridge retains its source geometry. A 2.67% outer approach and supporting foundation connect it to the modern-context terrain. These are reversible D-confidence engineering interventions, not claims about a Qing gate datum. Foliage and debris remain sparse according to the existing exclusion contract. Their shapes, colors and paving dimensions are interpretive assets pending historical art review.

The two-map cook profile removes four unrelated always-cook directories through command-line overrides. A pass does not certify the normal whole-project packaging configuration. Historical H1, H2 and Z remain blocked; see `Historical_Source_Followup.md`.

Current packaged results: 7/7 physical routes; performance p95 30.425 ms, p99 31.815 ms, RSS 1.001 GiB, zero failed WP cells. The CSV has 6,632 independently reconciled samples. Stable copies of the final build, cook, stage, native review and runtime logs are archived in `Logs/`. Procedural source regeneration is byte-identical across all 14 generated files. See `Review/M05_Final_Handoff.md` for acceptance limits.
