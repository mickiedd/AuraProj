# BungeeMan weapon attachment alignment - 2026-08-15

## Intent

Correct the BungeeMan assault rifle's unnatural position and rotation using Unreal Editor Remote Python.

## Changed behavior

- The rifle was attached to `hand_l` through `WeaponHandSocket`, but that socket had a zero location and zero rotation. The rifle therefore appeared low and flat beside the character.
- Remote preview tuning selected a `-35 cm` socket-local Z offset and a stored socket rotation of `Pitch=90`, `Yaw=0`, `Roll=15`.
- The `WeaponHandSocket` was updated and saved on both `SKM_BungeeMan` and its skeleton. The existing runtime `SnapToTargetNotIncludingScale` path now receives the corrected transform without a native code change.

## Validation

- Remote Python re-read the saved socket as location `(0, 0, -35)` and rotation `(90, 0, 15)`.
- A temporary body-plus-rifle preview rendered the weapon raised and angled across the torso; preview actors were cleaned from the editor afterward.
- The rifle's `Muzzle` socket remained on `Grip_Bone` at its original location, so firing origin data was not changed.

## Visual summary

[View the weapon alignment diagram](./2026-08-15-bungeeman-weapon-attachment-alignment.svg)
