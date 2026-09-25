# Zhengximen realistic timber texture

**Date:** 2026-09-25  
**Illustration:** [timber before/after diagram](2026-09-25-zhengximen-realistic-timber.svg)  
**Viewport evidence:** [before](../../../Saved/Reports/Zhengximen/native-brick-plaque-after.png) · [after](../../../Saved/Reports/Zhengximen/native-timber-after.png)

## Intent
The user showed the pavilion timber reading as a uniform brown panel with repeated dark lines and requested more realistic timber texture.

## Changed behavior
- Rebuilt the six AgedWood and six DarkTimber source maps from a common anisotropic wood field. Warped lengthwise fibers, sparse knots, pores, soot and gray wear now vary color, height, normal, roughness and ambient occlusion together. The map is still nonmetallic and DirectX-normal oriented for Unreal.
- Increased the wall backing's UV repeat length from about 0.8 m to 3.2 m so the 18.8 m lower façade does not repeat the same grain over twenty times. Long DarkTimber beams also use a longer grain scale.
- Rotated UVs on vertical DarkTimber battens and the arched AgedWood door leaves so the grain follows the physical timber direction. The geometry and arch fit are unchanged.
- Reimported only the two timber mesh groups and twelve texture assets, then rebound their existing Blueprint components. The saved Blueprint retains the same seven groups and material instances.

## Validation
- Reviewed the updated timber in the native Blueprint viewport: the broad wall panels show longer, irregular grain and tonal wear; structural pieces retain darker separation. [After screenshot](../../../Saved/Reports/Zhengximen/native-timber-after.png).
- Source rebuild: all nine GLBs carry TEXCOORD_0; 0/8,800 timber-wall rays, 0/135 gateway rays and 0/450 door-seam rays pass through.
- Fresh UE 5.5 validation: **0 errors and 0 warnings**; 271,644 triangles and union bounds **5320 × 1080 × 1689.68 cm**, unchanged from the masonry pass.
- Python syntax, SVG XML and `git diff --check` pass.
