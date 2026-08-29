# Day 34 — Content and Config Validation Gate

Status: Planned  
Depends on: Day 21 manifest and Days 25–30 data contracts

## Goal

Treat JSON, XML, HTML/WebUI, and map/config references as source code and fail before packaging when content is unsafe or incomplete.

## Work

- Build one validator for JSON syntax/schema/version, duplicate IDs, cross-file references, role/ability/item/merchant links, numeric bounds, map aliases, staged paths, and all AbilityDefinitions XML discovery.
- Make smoke runners and packaging consume the same canonical manifest rather than maintaining divergent handwritten lists.
- Add malformed and missing-fixture cases with path, field, definition, and reason in the error.
- Document runtime-generated versus asset-authored surfaces and the supported fixture contract.

## Detailed execution contract

### Files to inspect or create

- **Validator:** planned `Scripts/ValidatePlayableCandidateContent.ps1` plus any small parser modules; it must be callable by fast and candidate stages.
- **Manifest:** planned `Content/Config/PlayableCandidateManifest.json`, generated from the Day 21 scope and actual staged files.
- **Content roots:** `Content/Config/RoleConfig.json`, `PopulationSpawnTable.json`, `BattleZones.json`, `CivilianWorkProfiles.json`, `ItemDefinitions.json`, `MerchantDefinitions.json`, `EconomyConfig.json`, `PickupDefinitions.json`, `ServerConnection.json`, all `Content/AbilityDefinitions/*.xml`, WebUI HTML/JS, and the canonical map/config references.
- **New output:** `Source/Aura/Private/Tests/AuraRoleBattleDay34Tests.cpp`, malformed fixture corpus, validator report, and `day-34-content.json`.

### Validation pipeline

The validator runs `discover → parse → normalize → schema/version → duplicate IDs → cross-reference → bounds → staged-path/case → runtime/asset classification → canonical manifest`. It must enumerate files from the filesystem/package rather than a handwritten allow-list. A clean manifest is the input for tests and packaging; no later stage rebuilds its own divergent list.

### Detailed steps

1. Inventory every JSON/XML/HTML/map/config root used by the four lane manifest and classify runtime-generated versus asset-authored surfaces.
2. Define schema/version and stable-ID rules for roles, abilities, population members, battle zones, items, merchants, offers, rewards, tutorial steps, ammo, and WebUI contracts.
3. Resolve cross-file references, including RoleConfig → active FireGun XML, ability graph nodes, item/merchant/reward IDs, population merchant bindings, map aliases, and staged paths.
4. Validate numeric bounds, required arrays, duplicate IDs, case-sensitive paths, unknown tags, unsupported versions, and mutually exclusive legacy/current shapes.
5. Add malformed/missing fixtures for every domain; each error includes path, field, definition ID, expected value/type, actual value/type, and reason.
6. Make the fast gate return nonzero before build/package publication; make the candidate stage audit the staged package against the same canonical manifest.
7. Prove runtime-created Civilian/marker fixtures are intentional and that no duplicate map/assets are introduced to satisfy stale wording.
8. Publish the manifest hash and validator result for Day 38; an invalid clean candidate cannot proceed.

### Named automation and commands

- Native/config tests: `SchemaVersion`, `DuplicateIds`, `CrossReferences`, `FireGunPath`, `MerchantRewardReferences`, `MapAlias`, `ExhaustiveXMLDiscovery`, `MalformedFixtures`, `StagedCase`, and `RuntimeAssetClassification`.
- Run `Scripts/ValidatePlayableCandidateContent.ps1 -Manifest ...` in fast mode and again against the staged candidate; run all malformed cases and verify nonzero exit.
- Required result: clean content passes with zero unknown references; every invalid fixture fails early with actionable diagnostics and no candidate artifact publication.

## Deep-review closure

- **Owner surfaces:** planned `Scripts/ValidatePlayableCandidateContent.ps1`, planned `Content/Config/PlayableCandidateManifest.json`, all `Content/AbilityDefinitions/*.xml`, existing JSON/XML/HTML/config roots, and the Day 21 scope manifest.
- **Required artifacts:** validator result JSON/XML with schema version, exhaustive file list, duplicate/reference diagnostics, and the canonical manifest consumed by tests and packaging.
- **Gate:** the clean production-content set passes with zero unknown references; every malformed/missing fixture exits nonzero with path, field, definition, and reason; the candidate pipeline cannot publish when validator exit status is nonzero.

## Validation and evidence

- Clean production-content pass plus intentionally malformed role, economy, population, battle-zone, ability, and WebUI fixtures.
- Verify the validator runs in the fast gate and the packaged staged-data audit.
- Confirm invalid content prevents a candidate artifact from being published.

## Completion gate

Every invalid candidate fails early with an actionable error, while the clean candidate manifest is exhaustive and reusable by tests/package scripts.
