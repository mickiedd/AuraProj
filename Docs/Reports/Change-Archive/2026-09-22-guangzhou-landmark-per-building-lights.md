# One independent light per building — Guangzhou landmark showcase

## Intent

> "in the GuangzhouLandmark shoucase level, each building should have a independent light."

Two choices were put to the user before building, and both answers shape what
follows:

- **Role** — the per-building light is **added on top of** the existing global key
  (7.0) / fill (2.0) / sky (1.4) lights, which are left exactly as validated. It is
  an accent a building owns, not a replacement for the shared lighting.
- **Type** — a **spot light**: aimable, shadow-casting, and confinable so one
  building's light cannot wash its neighbours.

## Changed behaviour

`Scripts/CreateGuangzhouLandmarkShowcase.py` gained one step,
`spawn_landmark_lights(entries)`, run after the ring is laid out. Every landmark
now gets exactly one shadow-casting spot light that:

- stands **in front of its own building on the plaza side** — the side the
  landmark faces — and above its roof, aimed at the middle of the building;
- is **attached to the landmark actor**, so it belongs to that building and moves
  with it (attachment is `KEEP_WORLD` on all three rules, so the light is not
  nudged by attaching);
- is labelled `Light_<key>`, tagged `GuangzhouLandmarkLight`, and filed under the
  `LandmarkLights` outliner folder;
- is **derived entirely from the building's own measured geometry**, so a landmark
  swapped for a larger or smaller variant keeps a correctly sized, correctly aimed
  light.

| Parameter | Value |
| --- | --- |
| facade clearance | `radius + 1500 cm` in front of the facade |
| height | `1.35 × building height` |
| aim height | `0.45 × building height` |
| outer cone | building's subtended half-angle + 10°, capped at 80° |
| inner cone | `0.45 × outer` |
| intensity | `3.0 lux × distance_in_metres²` candelas — a ratio, so scale-free |
| attenuation radius | `1.15 × (distance + building radius)` |
| source radius / temperature | 60 cm / 5200 K |
| shadows | on |

Intensity is computed rather than fixed because candelas are not scale-free: a
constant candela value would light a small gate and a large one very differently.
Solving `illuminance = intensity / distance²` for a target illuminance at the
building gives every building the same accent strength. 3.0 lux sits between the
global key (7.0) and fill (2.0) on purpose — the level pins exposure, and this
level was already clipping pale stone, so the accent adds definition without
lifting the whole ring into the clip.

### The nine lights

| Building | r | h | light dist d | outer cone | attenuation | intensity | clear of neighbour |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Zhengnanmen HighFidelity | 1595 cm | 2302 cm | 3725 cm | 35.4° | 6118 cm | 4163 cd | 18.0° |
| Zhengnanmen AAA V3 | 1504 cm | 2250 cm | 3623 cm | 34.5° | 5896 cm | 3938 cd | 18.9° |
| Xiaobeimen AAA V3 | 2704 cm | 2253 cm | 4668 cm | 45.4° | 8478 cm | 6536 cd | 11.2° |
| Xiaobeimen Production V3 | 2718 cm | 2215 cm | 4665 cm | 45.6° | 8491 cm | 6530 cd | 10.8° |
| Guidemen V5 4K | 3008 cm | 1974 cm | 4846 cm | 48.4° | 9032 cm | 7044 cd | 9.8° |
| Wuxianmen V5 4K Core | 1865 cm | 1594 cm | 3658 cm | 40.6° | 6351 cm | 4014 cd | 17.0° |
| Wuxianmen V5 FullPBR | 1865 cm | 1721 cm | 3704 cm | 40.2° | 6404 cm | 4116 cd | 17.1° |
| GreatNorthGate (Dabeimen) | 2120 cm | 2476 cm | 4251 cm | 39.9° | 7327 cm | 5421 cd | 14.0° |
| Zhenhai Tower | 2460 cm | 2085 cm | 4382 cm | 44.1° | 7867 cm | 5760 cd | 13.3° |

"clear of neighbour" is the honest isolation figure: the angle to the nearest
other landmark **minus that landmark's own angular span** (a gate 70° off the
axis still subtends about 20° from this light), minus this light's outer cone.
Measuring to the neighbouring building's *centre* alone would be far too
generous — it would report 24° of clearance where the true figure is 9.8°.

### Re-runnability

The builder destroys and rebuilds everything tagged `GuangzhouLandmarkShowcase`,
and the new lights carry that tag, so they are rebuilt too. `clear_previous()`
needed one change: destroying a landmark destroys its attached light, so the
actor snapshot contains actors that may already be gone by the time the loop
reaches them. Each per-actor step is now guarded, and the level is asserted clear
afterwards — the assertion, not the removal count, is what proves it.

## Guards (asserted by the builder before the level is saved)

- one light per landmark, and its attach parent is its own building
- outer cone strictly wider than the angle the building subtends
- attenuation radius past the far side of the building
- shadows on, and the intensity / cone values **read back off the component**
  match what was requested
- no other landmark's body inside any cone
- every landmark still grounded on the ground plane and inside the ground plane

The readback guards exist because a light that silently kept its default is
indistinguishable from a configured one in a screenshot.

## Validation

`Scripts/ValidateGuangzhouLandmarkShowcase.py`, reloading the saved level from
disk — **0 errors, 0 warnings**, 34 actors:

- 9/9 landmark lights present exactly once, each owned by its own landmark
- each light **re-derived independently** from the placement manifest's own
  recipe plus the landmark's recorded geometry, rather than compared against the
  numbers the builder wrote about its own lights
- placement error **0.0 cm** and aim error **0.0°** on all nine
- intensity / cone / attenuation match the re-derived recipe to within 1e-4
  relative
- lights stand nearer the plaza centre than the buildings they light (proving
  they are in front, not behind)
- isolation: **0 violations**, worst margin 9.81°
- ring, grounding and overlap checks from the previous job still pass unchanged

## Measured effect

`Scripts/CaptureGuangzhouLandmarkLights.py` captures the same views twice — once
as configured, once with every landmark light's intensity set to 0 — then
restores the intensities and reloads the level. The level is never saved, and all
nine intensities were verified back on disk afterwards.

| View | mean, lights on | mean, lights off | delta |
| --- | --- | --- | --- |
| Guidemen (eye level, 56 m) | 0.71172 | 0.63766 | **+0.07406** |
| plaza (Zhengnanmen front) | 0.83863 | 0.82158 | +0.01705 |
| hero (whole ring, 42 m out) | 0.60742 | 0.60509 | +0.00233 |

In the Guidemen pair the effect is plainly visible: with the lights off the gate
is a flat black silhouette, and with its own light on the wall masonry, the red
column row and the roof structure are all lit.

Zeroing the intensity is the only valid way to run this test. Hiding a light
**actor** does not disable the light, so a "hide the lights" comparison returns an
identical image whatever the lighting is.

### Per-building verification

The three wide views above prove the lights as a group but say nothing about whether
each light frames and lights *its own* building — only Guidemen and Zhengnanmen were ever
captured close up. `Scripts/CaptureGuangzhouLandmarkLightsPerBuilding.py` closes that gap:
the same on/off method, one pair per building, each camera placed from that building's own
geometry (`1.6 × radius + 2500 cm` out, at `0.55 × height`, aimed at `0.45 × height`) so
the pairs are framed identically and comparable.

| Building | camera | mean off | mean on | delta | pixels brighter |
| --- | --- | --- | --- | --- | --- |
| Zhengnanmen HighFidelity | 50.3 m | 0.3430 | 0.4484 | **+0.1054** | 31.4% |
| Xiaobeimen AAA V3 | 68.3 m | 0.3811 | 0.4656 | +0.0845 | 29.9% |
| Zhengnanmen AAA V3 | 49.1 m | 0.3766 | 0.4392 | +0.0626 | 26.9% |
| Zhenhai Tower | 64.4 m | 0.2960 | 0.3401 | +0.0441 | 23.5% |
| Guidemen V5 4K | 73.1 m | 0.4046 | 0.4457 | +0.0411 | 20.1% |
| GreatNorthGate (Dabeimen) | 58.9 m | 0.2821 | 0.3149 | +0.0328 | 19.2% |
| Xiaobeimen Production V3 | 68.5 m | 0.3485 | 0.3812 | +0.0326 | 22.4% |
| Wuxianmen V5 4K Core | 54.8 m | 0.4421 | 0.4544 | +0.0123 | 16.2% |
| Wuxianmen V5 FullPBR | 54.8 m | 0.3799 | 0.3878 | **+0.0079** | 16.3% |

**All nine are positive**, and the contact sheet shows the reason plainly: with the light
off each gate is a flat dark silhouette, and with its own light on the masonry, the red
column row and the roof tiles are all picked out. Exposure control was verified before
these frames were trusted (EV100 3.0 → 0.605, EV100 6.0 → 0.094). Nothing was saved and all
nine intensities were verified back on disk.

### The frame-mean delta was the wrong metric — corrected

An earlier version of this record read the whole-frame mean delta as light strength and
concluded that the two Wuxianmen variants were the weakest, "roughly a tenth of the
strongest". **That was wrong.** A whole-frame mean is dominated by how much of the frame the
building occupies, and Zhengnanmen HighFidelity (2302 cm tall) fills far more of its frame
than Wuxianmen FullPBR (1721 cm).

`Scripts/AnalyzeLandmarkLightContribution.py` recomputes the same frames on the right basis
— the mean brightening **on the pixels the light actually brightens**:

| Building | frame Δ | coverage | Δ on lit pixels |
| --- | --- | --- | --- |
| GreatNorthGate (Dabeimen) | 0.0328 | 20.1% | 0.7124 |
| Guidemen V5 4K | 0.0411 | 22.8% | 0.6837 |
| Wuxianmen V5 FullPBR | 0.0079 | 18.8% | **0.6825** |
| Wuxianmen V5 4K Core | 0.0123 | 20.1% | 0.6670 |
| Xiaobeimen Production V3 | 0.0326 | 23.7% | 0.6602 |
| Zhenhai Tower | 0.0441 | 24.2% | 0.6470 |
| Xiaobeimen AAA V3 | 0.0845 | 31.4% | 0.6445 |
| Zhengnanmen HighFidelity | 0.1054 | 30.9% | 0.6177 |
| Zhengnanmen AAA V3 | 0.0626 | 27.6% | **0.6111** |

The nine lights light their buildings **within 1.17× of each other**, and the ordering
reverses: the two Wuxianmen lights are third and fourth *strongest*, while Zhengnanmen —
which had by far the largest frame delta — is weakest on this basis. The 1.67× spread in
coverage is the framing effect and nothing else.

**So there is nothing to tune**, and the earlier recommendation to strengthen Wuxianmen was
based on a bad metric. The uniformity is the recipe working as designed:
`intensity = 3.0 lux × distance²` gives every building the same illuminance from its own
light whatever its size, which is exactly what a fixed candela value would not do. A
whole-frame mean delta should not be used to compare buildings of different sizes.

## Capture-harness defect found and fixed

Chasing why the bracket frames came back clipped turned up a real defect in this
project's capture path, which is worth recording because it invalidates any
brightness judgement made from showcase captures.

1. **The exposure pin did not reach the capture at all.** With the flags the
   showcase capture script used, pinning exposure to EV100 3.0, 4.0, 5.0 and 6.0
   produced frames with identical statistics (mean 222.51 / 240.99, clipped
   78.91% / 93.07%). An 8× change in exposure must change the image.
2. **Two of the three flags do not exist.** `b_always_persist_rendering_state`
   and `b_capture_on_construction` are not properties of this build's
   `SceneCaptureComponent2D` — the real name is
   `always_persist_rendering_state`, with no `b_`. Both were set inside
   `try/except`, so the failure was swallowed and the script reported a
   configuration it had never applied.
3. **`capture_every_frame = False` is what breaks exposure control.** Measured
   directly (`Scripts/ProbeShowcaseCapturePersistenceFlag.py`): with default
   flags, EV100 3.0 and 7.0 separate by a factor of sixty (means 1440.5 vs 23.9);
   with `always_persist_rendering_state = False` they do not separate at all
   (17831.4 vs 17831.5).
4. **The archived 2026-09-21 bracket was sound.** `BracketShowcaseExposure.py`
   never set those flags, and its frames do respond to exposure (means 215.13 /
   199.96 / 85.32 / 24.80). So the level's EV100 3.0 pin rests on a working
   bracket; it is the captures taken since, through the flag-setting path, that
   did not.

**Fix:** the flags are removed from `CaptureGuangzhouLandmarkShowcase.py` and
`BracketShowcaseLandmarkLight.py`, and
`CaptureGuangzhouLandmarkLights.py` now **verifies exposure control before
trusting any frame** — two exposures are captured first and must separate
(measured: EV100 3.0 → mean 0.838, EV100 6.0 → mean 0.094), otherwise the script
fails loudly instead of emitting frames that look authoritative and are not.

**Superseded, deliberately not fixed:** `CaptureDadongmen*.py` (five scripts) and
`CaptureZhengnanmenManual4K.py` also set `capture_every_frame = False`, but all six write
their images with `export_render_target`, which this project established writes nothing in
this build. They are legacy scripts that produce no output at all, so the flag is moot
there.

## Open items

- **The bright blue lower half is the ground plane, and it is correct.** Ray traces
  (`Scripts/ProbeShowcaseFrameGeometry.py`) hit `Showcase_Ground` at 90 m, 28 m and 15 m
  for rows 560, 700 and 860 of the eye-level frame, and straight down. The ground is dark
  paving lit cool by the sky light, so it reads blue; the saturated cyan seen at EV100 3.0
  was the red channel clipping first on a cool surface, not a different surface.
- **A thin band of the frame is not geometry at all.** Row 510 (pitch −0.5°) hits nothing.
  That camera sits 50 m from the plaza centre and the 420 m ground plane ends 160 m away,
  so the ray leaves the plane's footprint long before it reaches the ground: that band is
  the SkyAtmosphere seen below the horizon. Both are blue, so the ground plane's edge is
  invisible and the blue reads as continuous. From eye level the ground plane is too
  small — worth enlarging if this level is ever viewed on foot.
- **An earlier version of this record claimed "no exposure yields mid-tones". That was
  wrong**, and it came from the broken-harness bracket whose four frames were all the same
  uncontrolled exposure. With the harness fixed, EV100 4.0 puts 30% of pixels in the
  mid-tones and the scene is legible: dark sky with a warm horizon band, lit roof tiles,
  red column rows, blue ground. The harness is still trustworthy for **relative**
  differences only.
- The global key/fill were deliberately left unchanged. If the per-building
  lights should become each building's *key* rather than an accent, dial the
  global pair back and re-check the exposure — the recipe constants are all at the
  top of `CreateGuangzhouLandmarkShowcase.py`.

## Files

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — per-building lights added
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — light checks added
- `Scripts/CaptureGuangzhouLandmarkLights.py` — new, self-validating on/off A/B
- `Scripts/BracketShowcaseLandmarkLight.py` — new, exposure bracket (flags fixed)
- `Scripts/ProbeShowcaseCaptureExposureControl.py`,
  `Scripts/ProbeShowcaseCapturePersistenceFlag.py`,
  `Scripts/ProbeSceneCaptureProperties.py`,
  `Scripts/ProbeLandmarkLightAPI.py` — the harness investigation
- `Scripts/ProbeShowcaseGroundCyan.py`, `Scripts/ProbeShowcaseFrameGeometry.py` —
  identifying what the blue lower half of the eye-level frame actually is
- `Scripts/CaptureGuangzhouLandmarkLightsPerBuilding.py`,
  `Scripts/RenderLandmarkLightContactSheet.py` — one on/off pair per building, and the
  contact sheet that lays all nine out
- `Scripts/VerifyShowcaseLandmarkLights.py` — final editor/level state check
- `Scripts/RenderLandmarkLightChangeDiagram.py` — renders this diagram from the
  manifests
- `Scripts/CaptureGuangzhouLandmarkShowcase.py` — capture flags fixed
- `Saved/RawModelImport/guangzhou-landmark-showcase.json` (light recipe +
  per-light records), `...-validation.json`, and
  `Saved/RawModelImport/GuangzhouLandmarkLights/` (A/B frames and reports)

[Visual summary diagram](2026-09-22-guangzhou-landmark-per-building-lights.svg)
