# AuraProj — UE / Git / build reference

Companion to `MEMORY.md`. Split out on 2026-09-25 because `MEMORY.md` had grown past the
size the agent injects, so its tail was being silently truncated — the traps below were
exactly the part being lost. Read this when a trap bites.

## UE 5.5 Python traps

1. `unreal.Rotator` positional order is **(roll, pitch, yaw)** — use keywords.
2. Hidden `Collision_UCX_*` sit **128 cm below the actor origin**: `get_actor_bounds`
   reports grounded actors sunk ~1 m. Measure rendered geometry, skip `visible == False`.
3. `HierarchicalInstancedStaticMeshComponent` derives from `StaticMeshComponent` —
   querying both double-counts every HISM.
4. `relative_transform` is unreliable on the V3 Blueprints (swaps local Z/Y) — use
   `relative_location` there. **Not general**: Wenmingmen's components sit at
   relative_location 0 with a -90° rotation and `relative_transform` *raises* on them.
5. `HitResult` fields are protected: `hit.to_tuple()` (4 = location, 8 = PhysicalMaterial,
   9 = hit_actor, 10 = hit_component). `line_trace_single` returns **None** on a miss.
6. `export_render_target` writes nothing: `SceneCapture2D` → persistent
   `TextureRenderTarget2D` → `RenderingLibrary.read_render_target` → PPM P6 → PNG.
7. One `capture_scene()` never converges auto-exposure: pin it with an unbound
   `PostProcessVolume` (both `auto_exposure_*_brightness` = one EV100), only while the
   capture keeps default flags (trap 9). `get_editor_property("settings")` returns a
   struct copy — write it back.
8. `set_actor_hidden_in_game` does **not** disable a light — zero its intensity instead.
9. `capture_every_frame = False` stops a SceneCapture2D honouring post-process, voiding
   the exposure pin. `b_always_persist_rendering_state` / `b_capture_on_construction`
   **do not exist** (real name `always_persist_rendering_state`). Unfixed in
   `CaptureDadongmen*.py`, `CaptureZhengnanmenManual4K.py`.
10. Judge showcase frames for **relative** differences only: the blue lower half of an
    eye-level frame **is** the ground plane (traces hit `Showcase_Ground` at 15-90 m);
    low-EV cyan is red clipping first. EV100 4.0 gives 30% mid-tones.
11. `EditorLevelLibrary` is gone — use `LevelEditorSubsystem` / `EditorActorSubsystem` /
    `UnrealEditorSubsystem`.
12. **`EditorAssetLibrary.delete_directory` can WEDGE the editor.** On a 176-asset folder
    the game thread froze inside `Force Deleting N Package(s)`: **0 CPU over 15 s**,
    `Responding` False, **no modal dialog** (main window visible *and* enabled) — a real
    block, not slow work. It returned True on a 56-asset folder once, so size-dependent,
    not always broken. Delete one asset at a time, leaf-first, logging each path *before*.
13. `find_package_referencers_for_asset(..., load_assets_to_confirm=False)` gates deletion
    but its edge list is partly **inverted** — treat as candidate discovery, confirm by
    loading the level and counting actors.
14. **A `StaticMeshComponent` with a component rotation breaks the obvious bounds read.**
    Wenmingmen is six `SMC_<part>` at relative_location 0 with a **-90° rotation** (Z-up
    GLB import), so `relative_location + mesh bounds Z` is wrong by a whole axis — it
    reported a local Z floor of **-2351 cm** where truth is **-332.5**, a 20 m error that
    reads exactly like a sunken building. Use each component's
    **`get_world_transform()`**; it reproduced the Blueprint bounds exactly.
    `unreal.PrimitiveComponent.bounds` is **not exposed**. `rendered_geometry` in
    `ValidateGuangzhouLandmarkShowcase.py` shares this bug (~-2019 for Wenmingmen) —
    reported, never asserted on. **The Blueprint bounds are the arbiter; if a measurement
    disagrees with them, fix the measurement.**
15. Facing is invisible to every geometric check — see "Placing a landmark in the ring"
    in `MEMORY.md`.
16. **FBX via Interchange can fail silently.** `SM_Zhengximen_LOD0.fbx` logged
    `Interchange start importing source` and never the matching `completed`, through both
    legacy `AssetImportTask`+`FbxImportUI` and `InterchangeManager.import_asset` — no
    asset, no error, no traceback, while four GLB imports succeeded in the same run.
    Working route: `InterchangeGenericAssetsPipeline` +
    `get_interchange_manager_scripted().import_asset(dest, source, params)` with
    `params.override_pipelines = [SoftObjectPath(pipeline.get_path_name())]`
    (`ImportGreatNorthGateHighDetail.py`).
17. **A Z-up GLB imports on its side, and axis heuristics cannot detect it.** Trimesh/
    Blender GLBs carry Z-up POSITION values against glTF's Y-up convention, so
    Interchange applies its Y-up-to-Z-up conversion and tips them over — at roll 0 every
    group returns with **Y and Z transposed** (AgedWood 18.86 × 8.64 × 14.70 m arrives as
    18.86 × 14.70 × 8.64). `IMPORT_ROLL = -90.0` restores the source;
    `import_offset_uniform_scale = 1.0` is right because Interchange already converts the
    glTF metres to cm. **Do not test "height exceeds depth"** — the brick wall is
    genuinely 1000 cm deep and 870 cm tall and the timber band 818 deep and 675 tall, so
    that test passes transposed parts and fails correct ones. **Judge against the GLB's
    own POSITION accessor bounds** (min/max in the glTF JSON) within a small tolerance.
18. `SubobjectDataSubsystem.k2_gather_subobject_data_**for**_blueprint` — "for", not
    "from". `MaterialEditingLibrary.get_material_instance_parent` **does not exist** —
    read `instance.get_editor_property("parent")`. A material path is
    `<folder>/MI_X.MI_X`, so match `"/MI_X."`, not a bare suffix.
19. **Take material assignment from the source's structure, not from slot names.** A
    package split into per-material meshes needs no name matching at all; the combined
    Zhengximen GLB had `"materials": []`, which is what ruled that route out.
20. `unreal.EditorStaticMeshLibrary.get_lod_count` / `get_simple_collision_count` are
    deprecated but still work. `unreal.FbxImportUI` option names (`combine_meshes`,
    `generate_lightmap_u_vs`, `auto_generate_collision`) are the documented spellings,
    but never assume they took — verify the resulting mesh.
21. **`EditorStaticMeshLibrary.get_num_uv_channels` always returns 0 — it is not a UV
    test.** It reports *source* UV channels, and an Interchange import carries no source
    data, so it returns 0 for every mesh including the known-good Wenmingmen and
    Zhengnanmen ones that render their 4K maps correctly. `StaticMesh.get_num_vertices`
    does not exist either (there is no `get_num_vertices` on the class at all). To prove
    a mesh can be textured, read the **source file's** vertex attributes — for a glTF,
    `TEXCOORD_0` in the primitive's `attributes`. That is what
    `ValidateZhengximenLandmark.py` now does. **A check that fails good assets is worse
    than no check** — probe an API against a known-good asset before trusting it.
22. **The size-based import test cannot see a MIRROR, so it can pass a gate that is back to
    front.** A mirror preserves every extent: `(x, -y, z)` and `(x, y, z)` give identical
    X/Y/Z sizes, so judging an import candidate against the GLB's own POSITION bounds
    (trap 17) matches to 2% while the model stands back to front. **Only a signed,
    asymmetric feature distinguishes a mirror from a rotation.** Zhengdongmen 2026-09-25:
    the import composes to `(x, -y, z)`, so its source facade on -Y arrived on **+Y** —
    proven by the plaque (source Y -598.2..-589 → imported +589..+598.2), the door studs
    (+204..+211.5 → -211.5..-204) and the stone base centre (-4.5 → +4.5), all agreeing.
    Everything else in that model is symmetric about XZ, which is exactly why a bounding
    box is blind to it. `ImportZhengximenLandmark.py` shares this blind spot.
23. **For a Z-up GLB only ONE roll gives the right up-axis, so its Y sign is not yours to
    choose.** Deriving the composition for Zhengdongmen: roll 0 → `(x, -z, -y)`, roll -90 →
    `(x, -y, z)`, roll +90 → `(x, y, -z)`, roll 180 → `(x, z, y)`. Three of the four put
    the building's **height on Y**, so roll -90 is forced and the Y negation comes with it.
    Do not go looking for a roll that fixes the facing — fix `facing_offset` instead.
24. **A model's facade can be on the opposite side to what its own package documents.**
    Zhengdongmen's README says `front = -Y` and its generator source proves the source data
    does; the import still delivered it on +Y. Read the documentation to know what to
    *expect*, then measure the imported asset to know what you *got* — and write the
    expectation into a validator so the disagreement is loud. The first
    `ValidateZhengdongmenLandmark.py` run failed on exactly this, and the validator was
    right.
25. **A report field is a claim too.** `ImportZhengdongmenLandmark.py` computed
    `union_size_cm` as the max per-axis extent, reporting a height of 1663 cm against the
    true 1933 cm — the same trap the Zhengximen record already documented, reintroduced.
    Union the parts' **absolute** bounds, never the largest extent per axis.

26. **A GUI editor launched from this shell stalls at plugin mounting unless it gets
    `-unattended`.** `Aura.log` stopped dead right after `SourceControl: Revision control is
    disabled` and sat there for **ten minutes** with no further output and no growth — it
    looked exactly like a slow project load. The identical command line plus **`-unattended`**
    loaded, captured all seven views and exited in **under a minute**. Nothing was wrong with
    the project; the first launch was waiting on a modal. `-unattended` does not disable Slate
    post-tick callbacks, so the capture script's tick still fires. Launch native captures as:
    `UnrealEditor <project>.uproject -ExecCmds="py <abs>/Scripts/RunZhengdongmenNativeReview.py"
    -stdout -nosplash -unattended -NoSound -AbsLog=<abs>/courses-native.log`.

27. **An open GUI editor blocks the next job, so a review process must quit itself.** The
    2026-09-25 native-review process never exited; its leftover editor held the project for the
    whole of the next session and blocked the isolated reimport commandlet that the roof-course
    rebuild needed. `RunZhengdongmenNativeReview.py` now calls
    `unreal.SystemLibrary.quit_editor()` in its `finally`. Do not treat a running editor as a
    neutral end state — check `pgrep -f UnrealEditor` before starting any commandlet work.

28. **A texture that already exists is never re-imported, so a re-authored image silently
    keeps its old pixels.** `import_textures()` imports only when the asset is missing, and
    re-tunes compression/sRGB on the ones it finds — so a regenerated BaseColor with **new
    dimensions** (the Zhengdongmen plaque moved from a 4096² square to 2048 × 512) would never
    reach the engine. Use the `refresh_textures(names)` entry point, which reimports with
    `replace_existing=True` and then re-applies `MAP_KINDS`. Verify from the report's recorded
    `size`, not from the fact that the run passed.

29. **A new material group has no material instance, and nothing tells you.** The instance
    builder (`build_instances`) lives in the import script's `main()`, which the *apply* path
    does not call — it only reimports meshes and rebinds components. So adding a group gives a
    mesh with a slot pointing at `MI_<Group>` that does not exist, and the only trace is a
    single `LoadAsset failed` line in the log. Call the import's **idempotent
    `build_instances(master)`** from the apply path; it creates the missing instance and
    re-binds every group's textures at the same time.

30. **A swept beam's end caps need their own UVs, or the mesh imports with degenerate
    tangents.** A cap's plane is perpendicular to the beam axis, so the default world-XZ
    projection gives every one of its vertices the same `u` — Interchange then logs
    *"degenerate tangent bases which will result in incorrect shading"* and *"nearly zero
    bi-normals"*, and those faces shade wrong. Give each cap a planar UV in the section's own
    plane. (Zhengdongmen's ridge beams: 2 warnings → 0.)

31. **Migrate a Blueprint in place when a level references its class.** A wrapper creator that
    keys on the component count (`if hism_count(existing) != len(MESH_GROUPS): delete_asset`)
    will **force-delete and recreate** the asset whenever a part is added — and the showcase
    level holds a reference to that Blueprint's generated class. Add the missing component to
    the existing asset instead. Related: `delete_asset` is a force-delete everywhere, so it is
    never the right first move on an asset that something else points at.

32. **A replacement feature should fill the same bounding box as the one it replaces.** Swapping
    Zhengdongmen's square ridge beams for half-round caps kept the envelope *exactly* — a
    semicircle of radius `width/2` on a `width/2` base occupies the same box as the square — and
    the check that proved it was comparing the new part's recorded size against the old part's:
    **2697.36 × 1408.97 × 722.15 cm, identical**. Designing a silhouette change that way avoids
    dragging the declared dimensions, the union assertion and the ring placement along with it.

33. **Put the "is an editor running?" check in the script, not in your head.** An apply
    commandlet that ran while a GUI editor held the project still produced correct disk assets —
    but the editor kept stale copies of the meshes and the Blueprint, and could have saved them
    back over the fix. Detect it with `subprocess.run(['pgrep', '-f', r'UnrealEditor\.app'])`.
    **The `.app` is load-bearing**: the commandlet's own path is `UnrealEditor-Cmd`, which also
    contains "UnrealEditor", so a bare pattern matches the commandlet itself and the guard always
    fires. A `subprocess.run` call works fine inside UE's Python. Note the sandbox here refuses
    to *spawn* a process named `.../UnrealEditor.app/...`, so the firing path has to be reasoned
    from observed command lines rather than reproduced.

34. **A 庑殿 hip roof's ridge is a line, not a flat top — and only a view down the roof shows it.**
    Zhengdongmen's upper slopes started at `y = ±0.5`, leaving a 1.0 m flat band of bare substrate
    at the top that the 0.30 m ridge cap could only partly cover: **0.35 m exposed each side**.
    Straight-on and three-quarter views cannot see this at all — it needs a camera above and off
    one end, aimed along the ridge. **Every fix so far was verified from a view that could not
    have shown the next defect**, so when a user reports something from an angle you have never
    rendered, add that angle to the preview renderer *and* the native capture before diagnosing.
    Fixing it also makes the east/west faces true triangles (`half_width_at(0)` → 0), which drops
    their uncovered-strip metric to zero.

35. **A V3 asset may have no source at all — read the geometry out of the engine.** Xiaobeimen
    AAA V3 is a Blueprint of 104 subobjects with binary meshes. `StaticMeshDescription` is
    **per-element**: `get_vertex_position(VertexID)`, `get_triangle_vertex_instances`,
    `get_vertex_instance_uv`, `get_vertex_count`, `get_triangle_count`. There is **no**
    `get_vertex_positions`, and `VertexID` exposes **no id accessor** in this build, so dump
    positions and per-triangle UVs and skip indices — that is enough for every measurement these
    jobs need (an arch's intrados, a part's box, a UV scale). `AssetTools.export_assets` takes
    **two** arguments (an array of asset *paths* and a destination) and produced **FBX**, not OBJ;
    `unreal.TextureExporterPNG` exists but `export_asset_tasks` does not. Also: the V3 components
    carry a **-90 roll** and their mesh-local space is `(actor_x, actor_z, -actor_y)`.

36. **Interchange's roll→axis mapping must be measured per source, not reasoned from the last one.**
    A GLB authored for this pipeline landed 100× too large *and* on the wrong axes: glTF positions
    are **metres**, so a file carrying centimetres needs `import_offset_uniform_scale = 0.01`; and
    at **roll 0** Interchange maps the file's `(X, Y, Z)` to mesh-local `(X, Z, Y)`, which is why
    the Xiaobeimen door source is authored as `(actor_x, -actor_y, actor_z)`. Deriving it from the
    Zhengdongmen convention `(x, -y, z)` gave the wrong answer twice. **Have the import script try
    the four rolls and keep the one whose box matches a known target**, so a future re-authoring
    reports where it went instead of silently fitting. Compare against the **mesh-local** box, not
    the actor box — the -90 roll means they differ.

37. **A material with no parameters still has a graph you can edit from Python.**
    `get_material_expressions` does **not** exist, but `get_material_property_input_node(material,
    MaterialProperty.MP_BASE_COLOR)` does, so the graph can be walked from its outputs;
    `create_material_expression`, `connect_material_expressions` and `recompile_material` all work.
    Xiaobeimen's `M_StoneWall` turned out to be five `TextureSample` nodes wired straight to their
    properties with **no TexCoord node**, so one added `TexCoord -> Multiply` feeds all five.
    **Adding nodes is not idempotent and the graph cannot be enumerated to check** — write the fact
    into the script's own report file and skip the step on a re-run, or the scale squares.

38. **Do not chain a commandlet and a GUI editor in one shell command.** A `fix; capture` chain
    silently skipped the fix: the report stayed stale and the "after" capture came out
    **byte-identical** to the "before". Run them as separate calls and check the report's mtime
    before believing a run happened. Related: a commandlet without `-stdout` sends its Python
    output to the editor log, not to your redirect, so the log looks empty for reasons that have
    nothing to do with the failure.

## Git

`.claude/memory/visual-change-archive.md` is **append-only** — one line per job, never
edited or deleted. Its conflicts are always append-vs-append at the same tail offset:
resolve as the **union of both sides**, never picking one (prove via `git show :1:`/`:2:`/
`:3:` diffed against the base — 0 removed lines on both makes the union safe, then verify
the result is a strict superset). Same for `Docs/Reports/Change-Archive/`. *A stale
conflict block sat in the archive until 2026-09-25; check for `^<<<<<<<` when appending.*

- `git show <stage>:<path>` redirected to a file **writes CRLF** (`core.autocrlf=true` +
  `.gitattributes` `* text=auto`) — fix with `sed -i 's/\r$//'`.
- `grep -c $'\r'` is unreliable; count bytes: `tr -cd '\r' < file | wc -c`.
- `git diff --no-index` needs real paths; `grep -c` exits 1 on zero matches, breaking
  `&&` chains.
- `post-commit`/`post-merge` hooks can be cut off by a shell timeout *after* the commit
  succeeded — verify by inspecting the commit, not the exit status.

**A dirty `git status` on `*.uasset`/`*.umap` is usually a false positive — do not
"discard" it.** Committed blobs are **raw binaries** while `.gitattributes` declares
`*.uasset filter=lfs`, so the clean filter turns the working file into a ~130 B pointer
that can never match the committed binary. 2026-09-23: all 117 reported-modified files
had `git hash-object --no-filters <file>` equal to their index blob hash — byte-identical
to HEAD, zero real changes. Tells: `git lfs status` shows both sides equal (`Git: <oid> ->
File: <oid>`), and `git diff --stat` shows `Bin <big> -> ~130 bytes`. **`git restore .`
will not clean it** (the mismatch returns immediately). The fix is `git add --renormalize`
on the affected paths + commit, which converts the stored blobs to proper LFS pointers;
verify the LFS objects exist in `.git/lfs/objects` **before** committing. Done 2026-09-23
for the 117 files (`b2d258b`, still unpushed); if it recurs for new assets, same fix.

## Build

`EngineAssociation` is a **GUID**, so `resolve_engine_root` looks for `UE_{GUID}` and
fails, and `/Users/Shared/Epic Games/UE_5.5` has no `Mac/Build.sh`. Always override:

```sh
cd /Volumes/M2/Works/AuraProj && UE_ENGINE_ROOT="/Volumes/M2/Engine/UE_5.5" ./BuildEditor.command
```

UE 5.5.4 at `/Volumes/M2/Engine/UE_5.5`; incremental editor build ≈ 80 s (18 actions).

39. **A jaggies report is about the coarsest arc, which is usually not the one you authored.**
    Xiaobeimen's door arc was already at 1 cm chords; a 1 cm chord on a 265 cm radius deviates from
    the true arc by **0.0005 cm**, a thousandth of a pixel — so refining it could not have changed
    anything. The stepping was the **shading of the wall's arch cut**, `Wall_ArchSpandrel`, at ~4°
    / 18 cm chords. Measure **every** curve before refining one, and do the arithmetic:
    `sagitta = R(1 − cos(Δθ/2))`. A chord that looks big can be sub-pixel; a chord that looks
    small can be the whole problem.

40. **An unwelded mesh can be reconstructed from a dump that has no indices.** If
    `vertex_count == 3 × triangle_count`, the mesh is fully unwelded and triangle *t* uses
    positions `3t..3t+2` — so positions plus per-triangle UVs are enough to recover the topology.
    That is what let `Wall_ArchSpandrel` be rebuilt from its own geometry rather than guessed at,
    and it means skipping the `VertexID` accessors costs nothing.

41. **Rebuild a swept panel by sampling the curve by ANGLE, and let the geometry decide the split.**
    Xiaobeimen's spandrel is `{(x, z) : |x| <= 260, z_arc(x) <= z <= top}` — and that region is
    empty where the arc rises above the top, which is exactly what makes it *two* panels. Sampling
    by angle and dropping the points above the top produces the split for free, with no
    special-casing.
