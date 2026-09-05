# Day 57 — Content Authoring and Staged Validation

Status: Technical contract foundation implemented; source/staged hash validation and atomic registry tests pass. Full sign-off remains BLOCKED on the independent second-author packaged walkthrough.
Depends on: Days 42–56 schema owners and supported content inventory.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Let a designer compose a valid mission variant safely and catch malformed content before a player enters a run.

## Exact change surfaces

- New: `Content/Config/GameplayExpansionManifest.json`, `Scripts/ValidateGameplayExpansionContent.ps1`, `Docs/Reference/Gameplay-Expansion-Content-Authoring.md`.
- Extend planned `Source/Aura/Public/Gameplay/AuraGameplayDefinitionRegistry.h` and implementation; all gameplay JSON/XML definitions and fixtures.
- Existing: `Scripts/ValidatePlayableCandidateContent.ps1`, packaging/staging config under `Config/`, ability graph loader and existing UE asset validation path. Retain old manifest scope semantics.

## Data and authority contract

Validate source and staged definitions with the same schema/reference rules. Manifest includes gameplay profile, definition/version hashes, assets, map/anchor IDs, supported combinations and required tests. Registry parses once, validates everything, then publishes one immutable generation; no partial registry. New content is JSON/XML definitions referencing allowlisted executable node types and cooked assets, not arbitrary code or native class paths from clients.

## Numbered implementation steps

1. Integrate all per-day validators into one staged gate; check unique IDs, schema versions, bounded numbers, role applicability, DAG reachability, spawn budgets, objective compatibility, ability costs and status caps.
2. Resolve every soft asset/class/tree/material/mesh/audio reference in Editor and packaged content. Check mount paths, case-sensitive artifact names and raw JSON/XML staging rules.
3. Reject invalid pools, duplicate reward versions, unsupported save/content versions and incompatible profile combinations; report exact file, record ID, field and reason.
4. Write an authoring recipe that adds one training encounter using existing archetypes/objective verbs with no C++ changes. Include full minimal row, expected errors and verification command.
5. Have a second developer/designer follow the recipe in an isolated fixture; run the variant in packaged play, then exclude the demonstration fixture from release inventory unless explicitly accepted.
6. Produce content inventory showing four enemy counters, eight augments, three templates, two layouts, two mutators and one boss; count only validated playable entries.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay57Tests.cpp`; namespace `Aura.Gameplay.Day57`.

- StagedMatchesSourceHashes — changed/missing staged JSON/XML produces nonzero.
- InvalidReferenceDiagnostic — unknown ability/tree/asset reports precise field and refuses readiness.
- RegistryAtomicPublish — one malformed row leaves no partially usable generation.
- ContentBudgetValidation — oversize roster/radius/target count/negative timers fail.
- AuthoringWithoutNewCode — documented encounter variant loads and plays through the existing contract.
- LegacyManifestIsolation — old candidate manifest remains valid without new gameplay profile assumptions.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 57 -Stage Fast -RunId d57-fast
./RunGameplayExpansion.ps1 -Day 57 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d57-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

AuthoringLab: malformed schema/unknown tag/missing asset/case mismatch/duplicate ID/cycle/insufficient augment pool/oversized spawn list; source and cooked package lanes. Second-author test uses exact recorded content hash.

## Failure and timeout semantics

Registry validation has a 30s load budget; timeout or missing asset refuses run readiness with no default fallback. Packaging missing raw definitions is FAIL. Second-author availability is a separate BLOCKED usability row, not a forged pass. A passing staged technical gate may feed subsequent technical implementation/performance work while this human row remains explicitly open; neither Day 57 nor final usability becomes complete until the walkthrough exists. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-57.json, source-staged-manifest-comparison.json, invalid-content-fixtures.json, content-inventory.csv and second-author walkthrough/report. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Every mandatory definition and referenced asset is staged and valid; a second author can compose/play a supported variant without new C++; bad content fails before run entry.

## Defer / anti-goals

No visual node editor, runtime hot reload, mod scripting, arbitrary class loading or replacement of existing ability graph tooling.
