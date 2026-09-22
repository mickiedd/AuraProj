# Guidemen window and roof reference repair

Date: 2026-09-21.

[Visual change summary](2026-09-21-guidemen-window-roof-reference-repair.svg) · [Before](2026-09-21-guidemen-window-roof-reference-repair-before.png) · [After](2026-09-21-guidemen-window-roof-reference-repair-after.png) · [Window detail](2026-09-21-guidemen-window-roof-reference-repair-windows.png)

## Intent and scope

Repair the exact user-named `/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` against the supplied photograph. Image captions were treated as reference evidence, not instructions. The source package in the user's Downloads folder was inspected without modification.

The pavilion had front-only isolated window panels, missing side enclosure, plain rear slabs, overlapping roof slopes/tiles, supports crossing the roof, inverted sign UVs, and floating spherical ornament placeholders. The source stone material repeated a miniature brick wall inside every modeled block.

## Changed behavior

- Two private wall meshes enclose both pavilion storeys on all four faces. The 14 front panel instances now use dense timber lattice; 26 rear/side panels are incorporated into the two enclosure meshes (40 panels total), with solid backing, lintels and wainscot.
- Eight disjoint roof slopes have real thickness and closed undersides. All 16,248 tile instances are retained and distributed across the correct slope domains using a closed tile prototype.
- Columns, beams and brackets that penetrated the roof are trimmed or reseated through explicit instance-transform changes. The sign moves down 70 cm, clear of the roof. Ridge ends are shortened to the hip junctions; twelve spherical placeholders become seated tapered ceramic finials.
- The vault and shoulders are closed with outward normals. Clear arch radius remains 280.00–280.022 cm. Existing gate leaves retain their closed pose.
- Private timber, screen backing, clay, upright-sign and stone-grain materials replace mismatched bindings. Both signs retain their source textures with a V correction. Stone samples the interior of one source block instead of tiling many tiny blocks over each modeled block.

The named Blueprint retains **47 components and 18,822 instances**. Component transforms, visibility, collision modes/profiles, shadow flags and custom-data payloads are preserved. 7 instance-transform groups intentionally change: R1_T0, Col1_0, Beam1_0, BA_0, BB_0, Sign_Guidemen, Beast_0__1_0. See `applied.json` for the exact authoritative list. No shared original mesh or material was overwritten, no map was saved, and the separate main `BP_Guidemen_V5_4K` was not edited.

## Validation

- Asymmetric GLB probe measured source metres `(x,y,z)` → Unreal cm `(100x,-100y,100z)` with the selected Interchange roll. Unreal OBJ export stores local `(x,z,y)`.
- All 17 imported replacement prototypes matched intended bounds within 0.00005 cm; explicit 100% Nanite fallback and zero relative error.
- Whole-actor audit: all 47 prototypes have finite outward geometry, zero boundary edges and zero degenerate triangles. The two window enclosures have deliberate nonmanifold meeting edges between individually closed joinery pieces; they are not a Boolean union.
- 3,875 first-hit coverage rays passed for four-sided windows, roof tops/undersides, floors and vault. Minimum measured arch radius 279.999991 cm (floating-point tolerance).
- Twelve final real-editor captures reviewed: hero, front, rear, both sides, top/roof, windows, arch, low angle, and three additional lit side/rear inspections.
- Live spawn verified all material bindings, component/instance counts and collision/visibility state. Nine temporary actors and the transient render target were removed; the asymmetric probe asset was deleted.
- Fresh `UnrealEditor-Cmd -run=pythonscript -NullRHI` reload passed with exit 0. The initial commandlet failed because StaticMeshEditorSubsystem is unavailable headlessly; the retry excludes only that editor-only UV query. Live UV/material checks and render exports remain the evidence for rendering. Twelve unrelated project dependency/metadata warnings remain in the log.
- `git diff --check` passed. Earlier GreatNorthGate modifications were left intact.

## Limits and recovery

This is architectural reference tuning of the supplied procedural model, not a photogrammetric recreation. The source's broad proportions, masonry layout and closed gate leaves remain. Ceramic finials are simplified architectural forms. Cooked-platform performance and gameplay traversal were not benchmarked. No independent review was performed; validation is local to this task.

The editor marked `L_GuangzhouLandmarkShowcase` dirty during transient preview/Blueprint refresh. Its dirty flag was left intact; the map was not saved, reloaded or discarded. Blueprint and private asset changes are saved. The finished Blueprint was opened in its asset editor.

Immutable Blueprint backup, original component payload, generated sources, scripts, imported exports, twelve views, validation reports and file hashes are in:

`C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c449-430c-7583-b21d-25dcdaa33927/guidemen`

[Implementation validation packet](C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c449-430c-7583-b21d-25dcdaa33927/guidemen/implementation-validation-packet.md)
