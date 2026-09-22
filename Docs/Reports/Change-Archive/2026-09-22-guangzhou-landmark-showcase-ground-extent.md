# Showcase ground extent — 420 m to 8000 m

## Intent

Found while verifying the per-building lights on the same day, not sought out. The
eye-level captures kept showing a blue lower half, and chasing what it actually was
turned up a second thing: a band of the frame that is not geometry at all.

## How it was found

Ray tracing, not looking. `Scripts/ProbeShowcaseFrameGeometry.py` traced the rows of the
Guidemen eye-level frame (camera 400 cm high, 50 m from the plaza centre) and hit
`Showcase_Ground` at 90 m, 28 m and 15 m for rows 560, 700 and 860 — so the blue lower
half **is** the ground plane, dark paving lit cool by the sky light, and the saturated
cyan at low exposure was simply the red channel clipping first on a cool surface. That
part was correct all along.

But **row 510 hit no geometry at all**. The camera sat 50 m from the centre and the
ground plane ended 160 m away, so that ray left the plane's footprint long before it
reached the ground. That band of the frame is the **SkyAtmosphere seen below the
horizon**. Because the ground and the atmosphere band are both blue, the plane's edge was
invisible and the blue merely read as continuous — which is why this had gone unnoticed.

The hero view was the visible symptom: the ring sat on a slab with black void beyond it.

## Changed behaviour

One constant in `Scripts/CreateGuangzhouLandmarkShowcase.py`:

| | before | after |
| --- | --- | --- |
| `GROUND_SCALE` | 420 | **8000** |
| ground half extent | 210 m | 4000 m |
| ground bounds | ±21000 cm | ±400000 cm |
| band height, eye level | 1.09° / **27 px** | 0.06° / **1 px** |
| plane edge in the hero view | in frame | at 4.2°, out of frame |

The size was set by measurement rather than taste. The band is
`atan(camera height / half extent)` tall, and at 900 px over a 36° vertical FOV that is
25 px per degree, so 210 m gives 27 px, 1000 m gives 6 px, and 4000 m gives 1 px. 8000 m
is the first size that both removes the band for practical purposes *and* pushes the edge
out of the hero frame.

The plane is `/Engine/BasicShapes/Plane` — a single quad with `M_ShowcaseGround`, which
is built entirely from constant expressions with no textures, so stretching its UVs over
8 km is harmless and the size costs nothing.

## What it does not change

- The ring, its radius (10612.1 cm) and every landmark position, unchanged.
- The nine per-building lights: their attenuation radii are 59–90 m, so a larger plane
  changes nothing about them.
- The labels, the player start, the global key/fill/sky, the exposure pin.
- `GROUND_HALF_EXTENT` is derived from `GROUND_SCALE`, and the validator's "every
  landmark fits inside the ground plane" check still passes against the larger extent.

## Validation

- Builder: `passed: true`, ground plane recorded at scale 8000.0.
- `Scripts/ValidateGuangzhouLandmarkShowcase.py`, reloaded from disk: **0 errors,
  0 warnings**, 34 actors, 9 landmark lights, isolation **0 violations**, ground bounds
  ±400000 cm.
- Captures re-taken at EV100 4.0: the hero view is now ground to all four edges with no
  void, and the eye-level band is a single pixel.

The exposure for those captures was chosen by measurement and the choice logic was
corrected in the same pass: when no bracket step met the clipping target, the script
previously fell back to the *least clipped* frame — which is the darkest step, the
opposite of useful. It now falls back to the frame with the **most mid-tones**. On this
level that selects EV100 4.0 (23.1% mid-tones) over 5.0 (3.4%) and 6.0 (0.0%).

## Files

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — `GROUND_SCALE` 420 → 8000, with the
  reasoning recorded inline
- `Scripts/ProbeShowcaseFrameGeometry.py` — the trace that found the band
- `Scripts/CaptureGuangzhouLandmarkLights.py` — exposure fallback corrected
- `Saved/RawModelImport/GuangzhouLandmarkLights/hero-lights-on-ev04.png`,
  `plaza-lights-on-ev04.png` — the re-captured views

[Visual summary diagram](2026-09-22-guangzhou-landmark-showcase-ground-extent.svg)
