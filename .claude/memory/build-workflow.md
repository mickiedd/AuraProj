# AuraProj macOS Build/Run Workflow

- Project root: `/Volumes/M2/Works/AuraProj`
- Unreal Engine 5.5 root: `/Volumes/M2/Engine/UE_5.5`
- Project file: `/Volumes/M2/Works/AuraProj/Aura.uproject`
- Build the editor target from the project root with `./BuildEditor.command`.
- Launch the project in Unreal Editor with `./RunEditor.command`.
- The scripts source `Scripts/macos/unreal-common.sh`, read the project `EngineAssociation`, and auto-detect the mounted `/Volumes/*/Engine/UE_<version>` layout. If detection fails, set `UE_ENGINE_ROOT=/Volumes/M2/Engine/UE_5.5`.
- For a packaged Mac client, use `./BuildMacClient.command`; for a dedicated server, use `./StartDedicatedServer.command`.
