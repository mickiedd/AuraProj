# Login Game Mode Setup Guide

## Overview
Created `ALoginGameMode` and `ALoginPlayerController` to automatically connect clients to a dedicated server (127.0.0.1) when the Login map loads.

## Files Created

### 1. LoginGameMode
- **Location**: `Source/Aura/Public/Game/LoginGameMode.h` and `Private/Game/LoginGameMode.cpp`
- **Purpose**: Game mode for the Login map
- **Key Feature**: Automatically sets `ALoginPlayerController` as the player controller class

### 2. LoginPlayerController
- **Location**: `Source/Aura/Public/Game/LoginPlayerController.h` and `Private/Game/LoginPlayerController.cpp`
- **Purpose**: Handles client-side auto-connection logic
- **Key Features**:
  - Executes `open 127.0.0.1` on BeginPlay/OnPossess
  - Only runs on local client (uses `IsLocalPlayerController()`)
  - Configurable server address and enable/disable toggle
  - 0.5 second delay to ensure UI initialization

## Editor Configuration

### Step 1: Set Login Map GameMode
1. Open the **Login** map in editor (`/Game/Maps/Login`)
2. In the **World Settings** panel (right side)
3. Set **GameMode Override** to `LoginGameMode`
4. Save the map

### Step 2: Verify Player Controller Class
- The `LoginGameMode` constructor automatically sets `ALoginPlayerController` as the player controller class
- No additional configuration needed

### Step 3: Customize Server Address (Optional)
If you need to change the server address from 127.0.0.1:
1. Open `LoginPlayerController` blueprint OR
2. In C++, modify the `ServerAddress` property in `LoginPlayerController.h`:
   - `FString ServerAddress = TEXT("127.0.0.1");` → change to your IP/port
3. Can also be overridden per-instance in the editor

## How It Works

1. **Player loads Login map**
   - Map spawns with `ALoginGameMode` as the game mode
   - Game mode sets `ALoginPlayerController` for controlling the player

2. **Player controller activates (BeginPlay or OnPossess)**
   - Checks if this is a local player controller (client-side only)
   - Sets a 0.5 second timer to execute the connection command

3. **Timer fires (ExecuteClientConnect)**
   - Executes console command: `open 127.0.0.1`
   - This travels the client to the dedicated server address

## Network Behavior

- **Dedicated Server**: GameMode runs, but player controller connection logic is skipped (only local players connect)
- **Client**: Automatically connects to 127.0.0.1 when Login map loads
- **Single Player**: Will attempt to connect (can be disabled by setting `bAutoConnectToServer = false`)

## Disabling Auto-Connect

If you want to disable auto-connect for testing:
1. Open `LoginPlayerController` properties in editor
2. Set `Auto Connect To Server` to `false`
3. Or modify in code: `bAutoConnectToServer = false;`

## Build Notes

Both files are part of the `Aura` runtime module and will compile with standard Aura module dependencies.
Make sure to regenerate Visual Studio project files and rebuild if compiler errors occur.

## Troubleshooting

**Issue**: Console window shows "Cannot find server"
- **Solution**: Ensure the dedicated server is running on 127.0.0.1 before launching the client

**Issue**: Connection command doesn't execute
- **Solution**: Check `Saved/Logs/Aura.log` for the line: "LoginPlayerController executing connect command:"

**Issue**: Players get stuck on Login map
- **Solution**: Verify the dedicated server is accepting connections
- Check server logs for connection attempts
