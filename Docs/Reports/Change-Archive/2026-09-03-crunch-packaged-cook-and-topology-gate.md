# Crunch packaged cook and topology gate

Date: 2026-09-03

## Intent

Cook and inspect the migrated Crunch Game package while keeping the missing packaged server topology explicit instead of treating a client-only archive as a complete multiplayer release.

## Evidence

- Explicit Aura-target `BuildCookRun` completed successfully for the Win64 Game target.
- The staged package contains the five Crunch skill montage families and the RuntimeV2 content required by the migration.
- The packaged Game client reached `StartupMap` without crash signatures.
- The packaged host/client runner failed closed because `AuraServer.exe` was not present in the archive; the Game Server Manager therefore could not claim a matching packaged server.

## Gate decision

The client package is a valid staged Game artifact, but the multiplayer topology gate remains open. Dedicated-server packaging and a matching host/client traversal must be completed before calling the migration's packaged release gate closed.

## Illustration

[Open the packaged cook and topology flow](2026-09-03-crunch-packaged-cook-and-topology-gate.svg)
