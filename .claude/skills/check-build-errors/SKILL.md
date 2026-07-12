---
name: check-build-errors
description: Inspect the latest UnrealBuildTool build log for Aura compile/link errors and fix them. Use when the user asks to "check build errors", "fix build errors", "check the build log", or says the build/Aura failed to compile.
---

# Check UnrealBuildTool Build Errors

Inspect the latest build log and fix any errors in the Aura source.

## The log file

`C:\Git\UnrealEngine-5.5\Engine\Programs\UnrealBuildTool\Log.txt`

This file is overwritten on each build, so it always reflects the most recent build. It is
typically large (>256 KB), so **do not Read it whole** — the Read tool will refuse. Use Grep
against the file path with the error patterns below.

## Step 1 — Find the errors

Grep the log for compile/link/fatal errors. Use a case-insensitive regex over the file path:

```
error C|error LNK|error MSB|fatal error|unresolved external|ErrorText|ERROR:
```

This catches MSVC `error C####` (compile), `LNK####` (linker), MSBuild errors, fatal errors, and
unresolved externals. Inspect with context (`-C 2`) so you see the `note:` / file path lines that
often follow a C++ error.

## Step 2 — Parse each error

Each Aura error line looks like:

```
[2/12] Compile [x64] AuraCharacterAnimInstance.cpp
C:\Git\AuraProj\Source\Aura\Private\Animation\AuraCharacterAnimInstance.cpp(33): error C2039: "IsCrouched": ...
```

- The first path + `(line)` is the offending file and line in **this repo** (`C:\Git\AuraProj\Source\...`).
  Fix those. Ignore errors whose path is under `C:\Git\UnrealEngine-5.5\...` — those are engine
  headers referenced in `note:` lines, not your code.
- **MSVC messages may be in Chinese** (this machine's locale), e.g.
  `不是 "UCharacterMovementComponent" 的成员` = "is not a member of". Translate as needed to
  understand the error; the `error C####`/`LNK####` code is the reliable part.
- Common Aura error codes:
  - `C2039` — identifier is not a member of the type (wrong API owner, e.g. calling a method on
    `UCharacterMovementComponent` that actually lives on `ACharacter`).
  - `C2065` — undeclared identifier (missing include or typo).
  - `C2664` / `C2440` — argument/conversion type mismatch.
  - `LNK2019` / `LNK2001` — unresolved external (missing `*.cpp` in build file, or missing
    `MODULENAME_API`, or unimplemented method).

## Step 3 — Fix and confirm scope

- Apply the fix in the Aura source file (use Edit).
- Re-grep the log to make sure there are no **other** errors you skipped — fix all of them before
  stopping. The build stops at the first failing module but the log may contain several errors
  from the same compile batch.
- Tell the user to rebuild to confirm (a full UE rebuild is slow; don't run it yourself unless
  asked).

## Notes

- Ignore stale VS Code/IntelliSense diagnostics shown by the IDE (e.g. `pp_file_not_found` /
  `undeclared_var_use` on a header that clearly exists). The **build log is the source of truth**;
  those red squiggles are an IntelliSense indexer quirk, not compiler errors.
- The log can also be checked after a live-coding compile; the same file is used.