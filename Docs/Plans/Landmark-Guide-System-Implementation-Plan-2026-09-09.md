# Landmark guide system — implementation status and next gates

Date: 2026-09-09. The first implementation slice is complete and compiled. The remaining gates are deliberately explicit because the checked-out project has no explicit marker actors yet and only BehaviorU editor binaries, without the plugin source/descriptor needed for a compiled BehaviorU agent binding.

## Implemented in this slice

- `AAuraLandmarkMarker` and `UAuraLandmarkWorldSubsystem` define stable IDs, display names, sort order, approach transforms, arrival radius, duplicate-ID rejection and world registration.
- The current showcase map is supported through a bounded fallback that discovers only `ZhenhaiTower` and `GreatNorthGate` actor tags. It projects a grounded entrance to navigation and marks rows unavailable when no navmesh exists; it never routes to a building centre or through collision.
- `AAuraPlayerController` owns a guide state machine with server request/cancel RPCs, synchronous complete-path validation, face-then-run movement using the existing spline and CharacterMovement input, arrival tolerance, a stuck watchdog, and cancellation on manual/ability input.
- `UAuraLandmarkPanelWidget` is a native UMG panel mounted by `AAuraHUD`. It lists the catalog, disables unavailable rows with a reason, exposes Stop and Close, and toggles with `L`.
- `Content/BehaviorTrees/BT_LandmarkGuide.xml` records the BehaviorU action contract (`ValidateGuideRequest`, `FaceLandmark`, `FollowLandmarkPath`, `ConfirmGuideArrival`) with real `BT_RUNNING` semantics for the future plugin source integration.

## Validation completed

1. `Build.bat AuraEditor Win64 Development C:/Git/AuraProj/Aura.uproject -WaitMutex` — pass.
2. `Build.bat Aura Win64 Development C:/Git/AuraProj/Aura.uproject -WaitMutex` — pass.
3. Initial implementation run (historical) — Unreal automation `Aura.Landmark` reported 2/2 pass (`Aura.Landmark.Marker.Contract`, `Aura.Landmark.Behavior.Contract`).
4. Initial implementation run (historical) — NullRHI showcase launch recorded both fallback rows as available and `[Landmark] Native guide panel mounted`.
5. Visual evidence — the native panel mounted during a rendered game launch; the available UI automation surface did not expose an Unreal window for screenshot capture. The SVG archive below is the durable visual summary, and rendered screenshot capture remains a Day 5 acceptance gate.

## Reapply validation — 2026-09-10

1. `Build.bat Aura Win64 Development C:/Git/AuraProj/Aura.uproject -WaitMutex` — pass; the freshly restored controller, registry, panel and contract-test translation units compile and `Aura.exe` links.
2. `Build.bat AuraEditor Win64 Development C:/Git/AuraProj/Aura.uproject -WaitMutex` — source compilation and UHT pass, but the final editor link is blocked by the already-open user-owned `UnrealEditor.exe` holding `UnrealEditor-BehaviorURuntime.dll`.
3. Live `Aura.Landmark` automation and rendered smoke — pending until that editor is closed and the new editor module can be linked; the attempt against the running editor's old module correctly reported no matching restored tests.
4. Standalone XML contract parse and scoped whitespace checks — pass. Prior map smoke evidence remains in the implementation validation packet; no user-owned editor process was stopped.

## Next gated work

1. Author `AuraLandmarkMarker` actors with safe entrance transforms and add/rebuild NavMeshBoundsVolume coverage. Verify Tower↔Gate and spawn↔each entrance paths in the saved level.
2. Restore the BehaviorU source/plugin descriptor in this checkout, add a player guide agent component, load `BT_LandmarkGuide.xml` on the owning controller, and bind the four methods through the game-thread command queue. Add queued-cancel and no-double-tick tests.
3. Add server-issued trip IDs, accepted route replication and server arrival acknowledgement; test two clients, stale responses, possession changes, death/mount, manual input and packet delay/loss.
4. Complete rendered panel QA at 1280×720, 1920×1080 and 2560×1440, then package client/server and verify XML staging.

Deferred: automatic dismount, teleport, cross-map travel, quest unlocks, minimap/search/narration and unverified landmark tags.
