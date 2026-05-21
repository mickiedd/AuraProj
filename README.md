# AuraProj

## Windows

Important prerequisite for dedicated server:

- Dedicated server targets on Windows require a source-built Unreal Engine installation.
- If packaging/building logs show "Server targets are not currently supported from this engine distribution", switch from Epic Launcher engine to source-built UE and set UE_ENGINE_ROOT accordingly.

Build dedicated server target:

```bat
BuildDedicatedServer.bat
```

Build and run dedicated server executable (not UnrealEditor server mode):

```bat
BuildAndRunDedicatedServer.bat
BuildAndRunDedicatedServer.bat /Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon -Port=7778 -QueryPort=27016
```

Stop dedicated server processes launched from either UnrealEditor server mode or built server executable:

```bat
StopDedicatedServer.bat
```

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

