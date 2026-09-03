"""Contract checks for the Crunch migration runner and frozen scope files."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load(relative: str):
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def main() -> int:
    failures = []
    scope = load("Docs/Plans/Crunch-Migration/crunch-migration-scope.json")
    assets = load("Docs/Plans/Crunch-Migration/crunch-source-asset-manifest.json")
    network_schema = load("Docs/Plans/Crunch-Migration/crunch-network-result.schema.json")
    required_inputs = {
        "InputTag.LMB": "Abilities.Melee.CrunchCombo",
        "InputTag.1": "Abilities.Melee.CrunchUppercut",
        "InputTag.2": "Abilities.Melee.CrunchDash",
        "InputTag.3": "Abilities.Melee.CrunchGroundBlast",
        "InputTag.4": "Abilities.Melee.CrunchTornado",
    }
    if scope.get("inputMap") != required_inputs:
        failures.append("scope input map is not the frozen five-skill contract")
    if scope.get("authority", {}).get("fallbackTimeline") != "dedicated-server-only":
        failures.append("fallback timeline is not dedicated-server-only")
    if scope.get("completionRequiresMatchingPackage") is not True:
        failures.append("completion must require a matching package")
    if scope.get("completionRequiresInWorldVisualEvidence") is not True:
        failures.append("completion must require in-world visual evidence")
    anchors = assets.get("requiredAnchors", [])
    if len(anchors) < 18:
        failures.append(f"source asset anchor inventory has {len(anchors)} rows; expected at least 18")
    if len({row.get("path") for row in anchors}) != len(anchors):
        failures.append("source asset anchor inventory contains duplicate paths")
    for row in anchors:
        if len(row.get("sha256", "")) != 64:
            failures.append(f"invalid SHA-256 for {row.get('path')}")
    expected_exports = {
        "dependencyClosure",
        "blueprintDefaults",
        "montageSectionTimes",
        "montageNotifyTimes",
        "gameplayCueReferences",
        "sourceClassReferenceScan",
    }
    if set(assets.get("requiredEditorExport", [])) != expected_exports:
        failures.append("source asset manifest does not freeze the six required editor export lanes")
    expected_scenarios = {"ComboFull", "ComboCancel", "ComboBeforeClose", "ComboAtOrAfterClose"}
    if set(network_schema.get("properties", {}).get("scenario", {}).get("enum", [])) != expected_scenarios:
        failures.append("network schema does not enumerate the four deterministic combo scenarios")
    if set(network_schema.get("properties", {}).get("eventSource", {}).get("enum", [])) != {"Authored", "DedicatedFallback"}:
        failures.append("network schema does not enumerate authored and dedicated fallback event sources")
    smoke_runner = (ROOT / "RunCrunchComboNetworkSmoke.ps1").read_text(encoding="utf-8")
    if "CrunchComboNetworkProbePreserveMovement" not in smoke_runner:
        failures.append("listen runner cannot retain normal movement replication for certification")
    if "CrunchComboNetworkProbeOffscreenAuthority" not in smoke_runner:
        failures.append("listen runner cannot exercise an offscreen authority mesh")
    semantic_validator = ROOT / "Scripts/Tests/validate_crunch_network_timeline.py"
    if not semantic_validator.exists():
        failures.append("network timeline semantic validator is missing")
    else:
        validator_text = semantic_validator.read_text(encoding="utf-8")
        for invariant in ("server must open exactly", "server must close exactly", "authoritative damage", "AtOrAfterClose"):
            if invariant not in validator_text:
                failures.append(f"network timeline validator is missing invariant: {invariant}")
    dedicated_preflight = ROOT / "RunCrunchComboDedicatedPreflight.ps1"
    if not dedicated_preflight.exists():
        failures.append("dedicated fallback preflight is missing")
    else:
        preflight_text = dedicated_preflight.read_text(encoding="utf-8")
        for contract in ("AuraServer.exe", "BLOCKED", "RequiresExternalClient", "NoListenSubstitution"):
            if contract not in preflight_text:
                failures.append(f"dedicated preflight is missing contract: {contract}")
    export_preflight = ROOT / "RunCrunchMigrationExportPreflight.ps1"
    if not export_preflight.exists():
        failures.append("source editor export preflight is missing")
    else:
        export_text = export_preflight.read_text(encoding="utf-8")
        for contract in (
            "requiredEditorExport",
            "missingExports",
            "hashMatches",
            "BLOCKED",
            "READY",
            "CrunchMigration\\Exports",
            "SourceEditorPreflight",
            "source editor preflight blocked export generation",
        ):
            if contract not in export_text:
                failures.append(f"source editor export preflight is missing contract: {contract}")
    source_editor_preflight = ROOT / "RunCrunchSourceEditorPreflight.ps1"
    if not source_editor_preflight.exists():
        failures.append("source editor environment preflight is missing")
    else:
        source_editor_text = source_editor_preflight.read_text(encoding="utf-8")
        for contract in (
            "Crunch.uproject",
            "CrunchServer.Target.cs",
            "failed to load because module",
            "observedBlockers",
            "SourceEditorPreflight",
        ):
            if contract not in source_editor_text:
                failures.append(f"source editor environment preflight is missing contract: {contract}")
    migration_runner = (ROOT / "RunCrunchMigration.ps1").read_text(encoding="utf-8")
    for scenario in ("Assets", "Uppercut", "Dash", "AreaSkills", "GroundBlast", "Tornado"):
        if scenario not in migration_runner:
            failures.append(f"migration runner does not gate scenario: {scenario}")
    if "source editor export preflight blocked scenario" not in migration_runner:
        failures.append("migration runner does not fail closed on missing editor exports")
    for required_probe_contract in (
        "AuthoredOpen Time=",
        "AuthoredClose Time=",
        "InputDecision Time=",
        "PredictionKey=",
        "TimelineCsv=",
    ):
        if required_probe_contract not in smoke_runner and required_probe_contract not in (ROOT / "Source/Aura/Private/AbilitySystem/Abilities/AuraMeleeAttack.cpp").read_text(encoding="utf-8"):
            failures.append(f"network probe is missing timing evidence contract: {required_probe_contract}")
    if failures:
        print("Crunch migration runner contracts: FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("Crunch migration runner contracts: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
