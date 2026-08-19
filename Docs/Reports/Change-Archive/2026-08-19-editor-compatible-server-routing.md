# UnrealEditor-compatible server routing

## Intent

Support the development flow where the client is running inside `UnrealEditor`, instead of assuming every client is the packaged `Aura.exe`.

## Changed behavior

- `GameServerClient` reports its executable, engine root, network changelist, and checksum to Game Server Manager.
- Game Server Manager recognizes `UnrealEditor` clients and launches the server from that same engine root.
- If a level is already running with the wrong server executable, the manager restarts that managed process before returning its endpoint.
- The existing packaged-server path remains unchanged for packaged clients; Unreal's strict network-version handshake is not bypassed.
- A detected Unreal network-version failure now tells the user to rebuild/restart the editor with the same project and engine build instead of presenting it as a generic connectivity problem.
- The AuraAbilityGraph/Aura intentional module cycle is declared to UBT, avoiding the previous generic editor game-module load failure during editor server startup.

## Evidence

- Existing editor log: `UnrealEditor` client `NetCL=37670630`, checksum `1419631917`.
- Existing packaged server log: `NetCL=0`, checksum `1081833902`; this explains the original code 6 failure.
- Matching `C:\Git\UE_5.5` editor server/client probe connected with checksum `1419631917` on both ends.
- A live Game Server Manager request carrying the editor metadata returned `{"status":"ready","serverMode":"editor","host":"192.168.1.6","port":7790}`; the managed server then logged checksum `1419631917` and the matching editor client connected without an incompatibility error.
- `Aura Win64 Development` compiled successfully, including `GameServerClient.cpp`.
- Editor routing contract, Game Server Manager contract, packaged compatibility gate, and prior Day 7/8/9 tests passed.

## Illustration

[Editor-compatible server routing](./2026-08-19-editor-compatible-server-routing.svg)
