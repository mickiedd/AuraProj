# Xiaobeimen AAA V3: wall stone, plaque height, arch-fitted door

Date: 2026-09-27

[Visual summary](./2026-09-27-xiaobeimen-aaa-fixes.svg)

Three defects the user reported together from one viewport screenshot and the reference
board: "the bricks of the walls need to be optimized", "the plaque's position is not
correct", "the wooden door also need to fit the arc perfectly".

`BP_Xiaobeimen_AAA_V3` is the **surviving** Xiaobeimen landmark — `Xiaobeimen_Production_V3`
was retired on 2026-09-22 — so this is the right asset to work on.

**The reference board is interpretive guidance, not a measured survey.** Every number below
was measured from the asset's own geometry, not read off the board.

## What the asset actually is

Unlike the Zhengdongmen work, this asset has **no procedural source**: it is a V3 Blueprint
of 104 subobjects whose meshes are binary `.uasset` in `Meshes/Xiaobeimen_HP/`. So the
geometry had to be read out of the engine before anything could be fixed. Two probes
(`Scripts/DumpXiaobeimenGeometry.py`, `Scripts/ProbeXiaobeimenAAA.py`) walk
`StaticMeshDescription` per element — `get_vertex_position`, `get_triangle_vertex_instances`,
`get_vertex_instance_uv` — and establish:

- the components carry a **-90 roll**, and the dumped meshes show
  `actor = (x_mesh, -z_mesh, y_mesh)`, so mesh-local space is `(actor_x, actor_z, -actor_y)`;
- the **arch**: springing at **z = 370 cm**, intrados radius **265 cm**, crown at **z = 635**,
  extrados 315 cm (a 50 cm ring);
- the wall's UVs run at **80 cm per unit** (`du/dx = dv/dz = 0.0125` on all twelve of its
  faces), so one 4K map covers 80 cm;
- the door leaves are plain rectangular blocks **146.6 cm wide with flat tops at z = 344.5**.

## 1. The wall stone was fine modern brick, not ashlar

Measured from a native capture by autocorrelating the render (camera 500 cm off the face,
hfov 34), the wall's stone units are **18 x 15 cm** — nearly square, and a third the size
the reference shows. It reads as fine brickwork laid on a castle.

Two changes together, because either alone fails:

- `Scripts/MakeXiaobeimenStoneTextures.py` authors a seamless coursed-ashlar set —
  **11 courses x 6 blocks** with per-course phase and per-block tone, blocks
  **32.6–83.0 cm wide by 24.8–32.1 cm tall** (mean 53 x 29), 2 cm mortar, and a height map
  whose joints are recessed so the normal map carries real relief. Seamlessness is by
  construction: courses span the full height and block boundaries the full width, so the
  joint at x=0 and the joint at x=W form one joint at the wrap.
- `Scripts/FixXiaobeimenAAA.py` inserts a `TexCoord -> Multiply(0.25)` pair into
  `M_StoneWall` and feeds all five of its texture samples, so one map covers **320 cm**
  instead of 80. Without this the ashlar blocks would have been 13 cm and the 80 cm repeat
  glaring.

`M_StoneWall` turned out to be five `TextureSample` nodes wired straight to their
properties with **no parameters and no TexCoord node**, which is why the scale had to go
into the graph rather than a material instance.

A first pass failed the other way: too much weathering noise swamped the coursing and the
wall read as flat camouflage. The amplitudes are now constants at the top of the script
with that recorded.

## 2. The plaque was floating in the arch's mouth

`Plaque_Body` spans z 497.5..612.5 and `Plaque_Face` z 513..597 — both **inside** the arch
opening, whose intrados at z = 497.5 is still ±232 cm wide. The tablet was hanging in the
archway rather than sitting on the wall.

It belongs on the wall above the crown. Between the crown (635) and the wall coping (787.5)
there is 152.5 cm, so a 115 cm tablet centred there sits at **z 653.5..768.5** — 18.5 cm of
clearance below and 19 cm above. The two components are set to an **absolute** target
(+156 cm on their authored offset) rather than nudged, so re-running the fix is a no-op.

## 3. The door did not fit the arch

The leaves were 146.6 cm wide with flat tops at z = 344.5, which is *below* the springing —
so the whole semicircular head stood open. Two 216 cm leaves cannot close a 530 cm opening
either.

`ContentSource/GuangzhouLandmarks/Xiaobeimen/source/build_xiaobeimen_door.py` is a new
source for a mesh that had none: two closed leaves whose **top edge is the intrados**,
`z(x) = 370 + sqrt(265^2 - x^2)`, rising from 370 at the jambs to 635 where they meet, 15 cm
thick, front face at actor y 445, plus a 7 x 10 stud grid per leaf that follows the arc. It
is exported as GLB and reimported over `Wooden_Doors__M_AgedWood` and `Door_Metal__M_Metal`.

**Two import conventions had to be measured, not assumed**, and each was worth a failed
attempt:

- glTF positions are metres, so a file carrying centimetres lands **100x too large** without
  `import_offset_uniform_scale = 0.01`;
- at **roll 0** Interchange maps the file's `(X, Y, Z)` to mesh-local `(X, Z, Y)`, which is
  why the source is authored as `(actor_x, -actor_y, actor_z)`. The fix script tries the
  four rolls and keeps the one whose box matches, so a future re-authoring reports where it
  went rather than silently fitting.

The leaves meet at the centre with **no gap**: any daylight between them shows the sky
through the unmodelled tunnel.

## Validation

- `Scripts/FixXiaobeimenAAA.py` re-run end to end: **passed**, no problems. Both door meshes
  land in their expected mesh-local boxes at roll 0, and the plaque reports `changed: false`
  on the second run — the idempotence check.
- The wall material's UV scale is recorded in the fix report and **skipped on a re-run**,
  because adding a TexCoord pair is not idempotent and the graph cannot be walked from
  Python (there is no expression enumeration).
- Six native Unreal/Metal captures before and after, cleanup 0 → 0.

| before | after |
|---|---|
| [wall](./2026-09-27-xiaobeimen-aaa-fixes-before-wall.png) | [wall](./2026-09-27-xiaobeimen-aaa-fixes-after-wall.png) |
| [arch](./2026-09-27-xiaobeimen-aaa-fixes-before-arch.png) | [arch](./2026-09-27-xiaobeimen-aaa-fixes-after-arch.png) |
| [front](./2026-09-27-xiaobeimen-aaa-fixes-before-front.png) | [front](./2026-09-27-xiaobeimen-aaa-fixes-after-front.png) |
| [three-quarter](./2026-09-27-xiaobeimen-aaa-fixes-before-three-quarter.png) | [three-quarter](./2026-09-27-xiaobeimen-aaa-fixes-after-three-quarter.png) |

## The door's pose is a judgement call

The reference photo shows this gate **open**, with leaves folded back and the passage
visible through the arch. The Zhengximen job used the same phrasing — "fit the arc
perfectly" — and what was wanted there was **closed leaves filling the arch**, so that is
what this does. The `Door_Metal` studs were rebuilt onto the closed pose, and the
2026-09-14 "outer edges at ±240 cm, mirrored 65° open" alignment is superseded. If the open
pose is wanted back, it is a re-run of the door generator with the leaves rotated, not a
revert of this work.

## Maintained source and saved assets

- Door source: `ContentSource/GuangzhouLandmarks/Xiaobeimen/source/build_xiaobeimen_door.py`
- Wall textures: `Scripts/MakeXiaobeimenStoneTextures.py`
- Fix: `Scripts/FixXiaobeimenAAA.py`; capture: `Scripts/CaptureXiaobeimenAAA.py`,
  `Scripts/RunXiaobeimenNativeReview.py`
- Probes: `Scripts/ProbeXiaobeimenAAA.py`, `Scripts/ProbeXiaobeimenPartsAndMaterial.py`,
  `Scripts/DumpXiaobeimenGeometry.py`, `Scripts/ExportXiaobeimenMeshes.py`
- Saved assets: `.../V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3.uasset`,
  `Meshes/Xiaobeimen_HP/Wooden_Doors__M_AgedWood.uasset`, `Door_Metal__M_Metal.uasset`,
  `Materials/M_StoneWall.uasset`, `Textures/StoneWall/StoneWall_*_4K.uasset`

## Open

- **The wall's 320 cm repeat is still visible** on a 30 m wall (about 9 repeats). The
  existing `Stone_SurfaceDetail`, moss and ivy meshes break it up, but a proper fix is
  texture bombing or a second detail layer, which is a material change beyond this ask.
- **Dark rectangular patches and dark spheres on the wall** read as holes. They are the
  pre-existing `Stone_SurfaceDetail` and vegetation/moss meshes, unchanged by this work, but
  they are the most obviously wrong thing left on the wall and worth a follow-up.
- The door's arc edge still shows slight stepping at very close range, and the arch ring's
  own tessellation sets a floor on how smooth that boundary can be.
- No knocker (铺首) on the new leaves — the previous `Door_Metal` carried studs only.
- `M_StoneWall`'s UV scale is now 0.25 for **every** part that samples it, including the
  approach ramp and the plaque body. That is consistent with the reference (large slabs
  throughout) but it is a global change, not a per-part one.

---

# Follow-up: the 锯齿 on the arch

The user, after the three fixes landed: "there're 锯齿 in that arc, try to increase the vertices
of the arc to fix this issues."

**The arc they were looking at was not the door's.** Measured before changing anything:

| arc | tessellation | chord on the intrados |
|---|---|---|
| door leaves (authored here) | 1.0 cm sampling, 527 columns | 1.0 cm |
| voussoir ring `Arch_Voussoirs` | 6° | 27.75 cm |
| **wall's arch curve `Wall_ArchSpandrel`** | **~4°, 38 angle samples** | **~18 cm** |

A 1 cm chord on a 265 cm radius deviates from the true arc by **0.0005 cm** — a thousandth of a
pixel at the capture's 0.63 cm/px — so the door's sampling could not have been the cause, and
refining it further would have changed nothing. The sagitta of even the ring's 27.75 cm chord is
0.36 cm, also sub-pixel. The visible stepping came from the **shading of the wall's arch cut**:
`Wall_ArchSpandrel` is the pair of spandrel panels that give the gate its arch, and their lower
edges were 18 cm facets.

`ContentSource/GuangzhouLandmarks/Xiaobeimen/source/build_xiaobeimen_arch_spandrel.py` rebuilds
it at **0.5° — a 2.31 cm chord**, 1,626 triangles against the original 432. Its shape was taken
from the original mesh (reconstructed from the dump: the mesh is fully unwelded, so triangle *t*
uses positions 3t..3t+2, which is what makes the topology readable at all) and from the arch
measurement:

    {(x, z) : |x| <= 260, z_arc(x) <= z <= 630.2},  z_arc(x) = 370 + sqrt(265^2 - x^2)

The region is empty where the arc rises above the panel's top, which is exactly what makes it two
panels rather than one — so the generator samples the arc **by angle** and drops the points that
go above the top. UVs are at the asset's own 80 cm per unit, so it matches every other stone part.

The door's arc was refined too, from 1.0 cm to **0.25 cm** (8,464 triangles), as asked.

## Validation

`Scripts/FixXiaobeimenAAA.py` re-run: **passed, no problems** — all three meshes
(`Wooden_Doors`, `Door_Metal`, `Wall_ArchSpandrel`) land in their expected mesh-local boxes at
roll 0.

[Before and after at pixel scale](./2026-09-27-xiaobeimen-arch-jaggies-before-after.png) ·
[full arch after](./2026-09-27-xiaobeimen-aaa-fixes-after2-arch.png) ·
[full gate after](./2026-09-27-xiaobeimen-aaa-fixes-after2-front.png)

## Open

With the arc smooth, the **dark rectangular patches and dark spheres on the wall** now stand out
more than they did — they are the pre-existing `Stone_SurfaceDetail` and vegetation meshes, and
they read as holes punched through the wall. Unchanged by this work, but they are the most
obviously wrong thing left on the gate.
