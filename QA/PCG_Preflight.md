# PCG preflight — 2026-09-24

**Result: unproven; use ordinary foliage instances for any prototype.** The project `Aura.uproject` does not enable `PCG`. UE 5.5.4 installation contains `Engine/Plugins/PCG/PCG.uplugin`, but no minimal project PCG asset has been loaded or cooked. Presence on disk is not a load/cook pass. The fallback is the plan's ordinary foliage instance path; a later switch to PCG requires an isolated smoke graph, editor reload and cook log with exit codes.

Read-only checks: `cat Aura.uproject`, `cat /Volumes/M2/Engine/UE_5.5/Engine/Build/Build.version`, `find /Volumes/M2/Engine/UE_5.5/Engine/Plugins -maxdepth 3 -iname '*pcg*'` (all exit 0). The already running editor and unrelated working tree were not modified for this probe.
