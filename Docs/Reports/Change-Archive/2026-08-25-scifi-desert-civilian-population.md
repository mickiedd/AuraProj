# Scifi Desert civilian population

## Intent

Add the already implemented AI-Civilian role to `Scifi_Desert_Level` so civilians are easy to encounter while testing village behavior.

## Changed behavior

- Added the `DesertVillageCivilians` population row with 32 initial and maximum civilians.
- Added and saved one deterministic spawn volume for each of the 32 outer villages.
- Added 48 village activity markers: 32 shelter, 8 work, and 8 observation markers.
- Excluded the largest central city cluster from the village topology.
- Made civilian volume bounds apply during construction and at runtime.
- Made population slots distribute across the configured village volumes instead of concentrating on the first volume.

## Validation

- AuraEditor Win64 Development build passed.
- Civilian AI, Day 7-9, and Day 10-12 focused contracts passed.
- Runtime server loaded `Scifi_Desert_Level` and finalized 32 live civilians.
- Runtime observed 32/32 population spawns, 32 Behavior Tree starts, 175 destination selections, 0 candidate-volume rejections, and 0 Behavior Tree startup failures.
- JSON, Python compilation, and `git diff --check` validation passed.

Illustration: [Scifi Desert civilian population flow](2026-08-25-scifi-desert-civilian-population.svg)
