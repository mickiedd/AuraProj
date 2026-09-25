# Zhengximen arch brick tiling

**Date:** 2026-09-25  
**Illustration:** [arch before/after diagram](2026-09-25-zhengximen-arch-brick-tiling.svg)  
**Viewport evidence:** [before](../../../Saved/Reports/Zhengximen/native-arch-before.png) · [after](../../../Saved/Reports/Zhengximen/native-arch-after.png)

## Intent
The user showed a gateway arch whose narrow repeated ribs and irregular stone pattern made the curved masonry look poorly tiled.

## Changed behavior
- Replaced the 96 longitudinal cylinder joints with 612 shallow curved tunnel bricks arranged in 25 staggered depth courses. Even 8 mm joints expose the continuous stone backing as mortar.
- Rebuilt each portal as 36 equally spaced curved voussoirs with approximately 8 mm radial joints, on both front and rear. Four subdivisions per stone maintain the circular opening without thin rib highlights.
- Mapped each dressed stone into a single stone region of the existing texture, avoiding a whole stretched texture grid on every wedge.
- Reimported only the changed `StoneFoundation` mesh into Unreal and rebound its existing Blueprint component. Other geometry, materials, and placement stay as before.

## Validation
- Native Unreal Blueprint viewport reviewed and saved in the after screenshot above: the portal has regular voussoirs and the tunnel reads as coursed brick, rather than striped tubes.
- Source geometry: 36 front and 36 rear voussoirs, 612 staggered vault bricks, zero rib cylinders. All nine rebuilt GLBs include `TEXCOORD_0`.
- Source ray tests: 0/8,800 pavilion-wall, 0/135 gateway, and 0/450 door-seam rays pass through.
- Fresh UE 5.5 validation: passed with zero errors and zero warnings; StoneFoundation has 7,876 triangles and landmark has 275,968 triangles.
