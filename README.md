# AuraProj

## macOS

This project was originally set up on Windows. On macOS, use the root-level scripts below after Unreal Engine 5.5 is installed.

The scripts auto-detect common Epic install locations and mounted-engine layouts such as `/Volumes/*/Engine/UE_5.5`.

If Unreal Engine is installed in a non-standard location, set one of these environment variables before running the scripts:

- `UE_ENGINE_ROOT=/path/to/UE_5.5`
- `UE_EDITOR_APP=/path/to/UnrealEditor.app`

Build the editor target:

```sh
chmod +x Scripts/macos/unreal-common.sh BuildEditor.command RunEditor.command StartDedicatedServer.command BuildMacClient.command
./BuildEditor.command
```

Launch Unreal Editor with this project:

```sh
./RunEditor.command
```

Launch the dedicated server or package a Mac client from Terminal:

```sh
./StartDedicatedServer.command
./BuildMacClient.command
```

