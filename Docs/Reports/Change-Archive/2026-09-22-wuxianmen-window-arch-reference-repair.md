# Wuxianmen FullPBR window, arch and door reference repair

Date: 2026-09-22. [Visual summary](2026-09-22-wuxianmen-window-arch-reference-repair.svg) · [Before](2026-09-22-wuxianmen-window-arch-reference-repair-before.png) · [After](2026-09-22-wuxianmen-window-arch-reference-repair-after.png) · [Windows](2026-09-22-wuxianmen-window-arch-reference-repair-windows.png) · [Arch](2026-09-22-wuxianmen-window-arch-reference-repair-arch.png) · [Door](2026-09-22-wuxianmen-window-arch-reference-repair-door.png).

## Scope and intent

Tune the exact `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR` against the supplied Wuxianmen sheet. Treat the sheet as visual evidence, not instructions. Original Downloads GLB and textures were inspected without modification. Other gates and the Core variant were not edited.

## Changed behavior

- Replaced incomplete window construction with 44 framed lattice screens across front, rear and both sides. Recessed opaque backing, wainscot, lintels, ceilings, lower panel infill and closed gables eliminate unintended sightlines.
- Separated roof tiers: upper tiles/decks rise 140 cm, lower tiles/decks lower 130 cm; the main window instances rise 170 cm. Supports extend to the upper roof. The plaque sits at Z 1305 cm, clear of the lower roof, with complete raised traditional lettering `門仙五`, read right-to-left as in the reference.
- Replaced the staircase arch boundary with a smooth continuous vault, 25 face voussoirs per end, trimmed adjacent masonry and closed passage floor. Clear radius 245 cm, spring height 325 cm. Twenty gate planks follow radius 242 cm, leaving 3 cm radial fitting clearance, and include iron fittings.
- Removed 563 intersecting arch masonry instances, 228 replaced timber/window/door/detached-tip instances and 8 floating ornament placeholders. Full original index lists remain in `removed-instances.json`. Four targeted repair components replace these parts; no duplicate complete building was added.
- Closed the noisy stone, timber, iron and tile prototypes with private convex envelopes and reprojected source UVs. The retained ridge prototype and its UVs remain intact. Decorative finials are seated on the eave ends. All new assets live under `ReferenceRepair20260922`.
- Fixed a broken supplied Roof_BaseColor PNG dependency and invalid/disconnected material graph inputs. Private rough stone/timber/clay materials use spatial texture projection to avoid stretched faces. Source normal maps exaggerated surface distortion and are not used by the final private graphs. The plaque uses geometry rather than a cropped image. Nanite is disabled for the thin raised plaque lettering; other new meshes have explicit full-triangle fallback with zero relative error.

## Validation

- Immutable baseline: 7 components / 5,103 instances, complete transforms, quaternions, custom data, materials and collision/visibility state, plus original Blueprint binary backup.
- Final: 11 static-mesh components / 4,304 instances. Retained instance order, custom-data subsets, planned transforms and original component collision/visibility/shadow settings pass exact or numeric comparisons. New visual repair components use NoCollision.
- Asymmetric probe verified source metres `(x,y,z)` to Unreal centimetres `(100x,-100y,100z)` using Interchange roll -90; OBJ export is `(x,z,y)` with winding conversion.
- Actual final Unreal exports: zero boundary edges, zero degenerate triangles, finite geometry, positive volume and outward winding. Lattice/enclosure joinery includes touching nonmanifold edges between independently closed pieces; this is not one Boolean manifold.
- **3,195 coverage rays pass** on four-sided enclosures, both roof tiers from above/below, floors, both door faces and vault. Imported arch radius: **244.9917927–245.0000000 cm**.
- Live spawn matches all saved mesh/material/instance bindings. Material graph checks confirm all nine spatial texture samples have valid textures and connected UV inputs.
- Eleven final live SceneCapture views reviewed: hero, front, rear, both sides, roof, windows/plaque, arch, door, low eave, floor/wall. Ten transient actors were removed and render target/texture residency released. No map was saved; the existing dirty map state was left intact.
- Fresh UnrealEditor-Cmd NullRHI reload: exit 0, serialized component payload matches; 0 errors and 12 unrelated project dependency warnings. NullRHI is not render validation; live images and exported geometry are the rendering evidence.
- Full topology, coverage, payload, live and fresh JSON results, exact scripts and source/asset hashes accompany the validation packet. `git diff --check` passes.

## Limits and recovery

This repairs the supplied procedural building, not a photogrammetric recreation. The broad masonry layout and two roof slope forms remain; small finials are simplified. Existing component collision settings are retained, but new repair components are visual-only, so gameplay collision/cooked performance need separate validation. No independent review was performed. The packet follows the current local-only implementation-validation skill.

Original Blueprint backup, generated GLBs, exported meshes, before/after captures, scripts and reports: `C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c4cc-cb1b-78b1-bd83-f6c5a46f6e08/wuxianmen`.

[Implementation validation packet](C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c4cc-cb1b-78b1-bd83-f6c5a46f6e08/wuxianmen/implementation-validation-packet.md).
