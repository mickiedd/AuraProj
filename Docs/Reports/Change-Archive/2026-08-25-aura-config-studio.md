# Aura Config Studio

Date: 2026-08-25

## Intent

Add a visual tool for inspecting and editing the JSON configuration files under `Content/Config` using the existing C++/embedded Web UI architecture.

## Before

The project had many runtime-consumed JSON files but no shared visual editing surface. Developers had to inspect and edit the files directly, with no project-wide file list, structured view, or common save guard.

## After

- Added `AuraWebUI.ConfigEditor`, which toggles an embedded `config-editor.html` page in a PIE or game world with a local player.
- Added a searchable file list for all JSON files under `Content/Config`.
- Added structured object/array editing and a Raw JSON fallback for complex documents.
- Added explicit `config_list`, `config_read`, and `config_save` bridge commands with request-correlated responses.
- Added the native `FWebUIConfigService` with path containment, `.json`-only access, JSON validation, root-shape checks, an 8 MB limit, optimistic content-version checks, `.bak` backups, and temp-file replacement.
- Kept domain-specific validation in the runtime systems; Config Studio is a generic JSON authoring surface.

## Validation

- `AuraEditor Win64 Development` build: passed.
- `Aura Win64 Development` build: passed.
- `AuraWebUI` automation: 4/4 passed, including config discovery/load, traversal rejection, invalid JSON rejection, stale-version rejection, and content contracts.
- Inline JavaScript syntax parse: passed.
- `git diff --check`: passed.

## Illustration

[Open the before/after Config Studio diagram](2026-08-25-aura-config-studio.svg)
