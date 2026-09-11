# Native GSM slow-start survival

## Intent

Stop the native Game Server Manager from terminating a healthy Unreal dedicated-server process merely because a client request reaches its shorter response deadline.

## Evidence and root cause

- The two supplied `Saved/Logs/GameServerManagerNative/Scifi_Desert_Level*.log` launches end abruptly during map loading, with no fatal, assertion, ensure, out-of-memory, or crash-report footer.
- The live native status API reported `World readiness exceeded 30-second launch deadline`, no process exit code, and exactly two launches. `Level::cleanup()` closed the kill-on-close job at that deadline, so GSM itself terminated each child.
- An isolated launch of the same editor server remained alive and reached `[WorldReadiness] State=Ready` after about 133 seconds.
- Two older `Aura` dumps are from a different installed-engine launch and assert while reading a one-byte-short `/Engine/EngineMaterials/WorldGridMaterial`; they do not match the supplied custom-engine GSM command line.
- The older Uppercut ensure dump is also not the current failure: current generated reflection contains `execHandleLaunchEvent`, and the current `UnrealEditor-Aura.dll` was rebuilt after that dump.

## Changed behavior

- The 30-second client request deadline now returns an explicit retry response without changing the level from `starting` or closing its job.
- The same process and readiness nonce remain authoritative, so later requests coalesce and a late authenticated `server_ready` notification is accepted.
- A separate, configurable 900-second server-startup deadline still bounds genuinely stuck launches and terminates their owned process tree.
- `requestDeadlineSeconds` and `serverStartupDeadlineSeconds` are validated from the top-level level configuration and reported in `/api/status`.

## Validation

- Release `AuraGSM.exe` rebuilt successfully with the Visual Studio/CMake project.
- `python Tools/GSM/tests/integration.py` — 12 tests passed, including request expiry without child termination, late readiness on the original PID/nonce, final startup-timeout cleanup, and invalid deadline configuration.
- Real Scifi Desert launch through the rebuilt manager: the first request returned at 30.045 seconds with `startup continues in the background`; status remained `starting`, `running=true`, PID `33812`, `launchCount=1`, and no fatal/assert/ensure signature. The same process reached `WorldReadiness=Ready` after 430.9 seconds with 32 live civilians, GSM accepted its first readiness callback, and a subsequent request returned `ready` in 0.037 seconds with the PID and launch count unchanged.
- `git diff --check` passed for the implementation changes.

## Visual evidence

- [Before/after lifecycle diagram](2026-09-10-native-gsm-slow-start-survival.svg)
