# macOS Unreal Engine stub repair

Date: 2026-09-03

## Intent

Make the documented `./BuildEditor.command` path recover from the missing Unreal Engine arm64/Development stub links that caused the AuraEditor link step to fail.

## Changed behavior

- Added a guarded preflight to `BuildEditor.command` through `Scripts/macos/unreal-common.sh`.
- The preflight verifies the four real UE 5.5 engine dylibs, creates only missing or dangling symlinks under the engine-generated stub directory, and fails if a required binary is unavailable.
- Existing real files at those paths are never overwritten.

## Validation

- `zsh -n Scripts/macos/unreal-common.sh BuildEditor.command` passed.
- `./BuildEditor.command` passed after repairing the links.
- The four links were then removed temporarily; a second `./BuildEditor.command` run restored all four automatically and passed the AuraEditor link/deploy step.
- `git diff --check` passed for the project changes.

The build still reports existing Unreal Engine deprecation warnings, but they are non-blocking and are unrelated to the missing-stub failure.

## Illustration

[macOS engine stub repair flow](2026-09-03-macos-engine-stub-repair.svg)
