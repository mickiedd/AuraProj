# Zhengximen masonry and plaque detail

**Date:** 2026-09-25  
**Visual summary:** [masonry and plaque diagram](2026-09-25-zhengximen-masonry-plaque-detail.svg)  
**Native viewport:** [before](../../../Saved/Reports/Zhengximen/native-material-repair.png) · [after](../../../Saved/Reports/Zhengximen/native-brick-plaque-after.png)

## Intent
Continue tuning BP_Zhengximen toward the supplied reference board: masonry should read as individual aged bricks, and the upper plaque should carry the reference's traditional Chinese text.

## Changed behavior
- Rebuilt the GrayBrick maps as a 4 m seamless tile with staggered 0.50 × 0.25 m courses, darker recessed mortar, stone-to-stone tonal variation, chipped edges, and matching height, normal, roughness and AO. The existing source material instance stays bound.
- Applied world-aligned UVs to all GrayBrick boxes. The 400 thin arch slices now share continuous UV coordinates with the wall instead of restarting the pattern on every slice.
- Replaced the old shallow front-only StoneFoundation overlay with individual GrayBrick facing on the front and rear of the central wall and both sides of the two wings. Faces stand roughly 5–6 cm proud of the continuous structural backing; the arch opening remains clear. Reimported StoneFoundation too, removing the obsolete overlay from the saved Blueprint.
- Regenerated the GatePlaque maps with **正西門**. Selected a Songti face containing the traditional 門 glyph and compensated for the plaque's wide aspect ratio so the characters read proportionately in Unreal. The plaque texture still maps once across its face.
- Added a focused reimport script for the twelve changed texture maps and the two changed mesh groups; the saved BP_Zhengximen retains seven material-group components and its original placement.

## Validation
- Native Blueprint viewport visibly shows separated gray masonry courses, weathered surface variation and a legible **正西門** plaque. Evidence: [after screenshot](../../../Saved/Reports/Zhengximen/native-brick-plaque-after.png).
- Rebuilt GLBs: all 9 written assets carry TEXCOORD_0. Source ray checks: 0/8,800 timber wall openings, 0/135 gateway openings, 0/450 door-seam openings.
- Fresh UE 5.5 commandlet validation: **0 errors, 0 warnings**, 271,644 triangles across seven components, expected union bounds **5320 × 1080 × 1689.68 cm**. The first validation identified stale StoneFoundation overlay triangles; reimporting that group removed them and made imported totals match the source exactly.
- Python syntax, SVG XML and `git diff --check` pass.

The board is a visual reconstruction reference, not a measured brick survey. Brick dimensions and face offsets are chosen for a convincing readable wall at the existing gate scale.
