"""Focused contracts for the verified Role/Battle review fixes."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def main() -> int:
    suite = read("RunReviewSmokeSuite2.ps1")
    assert "$script:results" in suite
    assert "$script:results.Add" in suite
    assert "TOTAL_STEPS=$($script:results.Count)" in suite
    assert "$script:results | Export-Csv" in suite

    smoke_batch = read("RunSmokeTest.bat")
    smoke_supervisor = read("RunSmokeTestSupervisor.ps1")
    smoke_module = read("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AuraAbilityGraphModule.cpp")
    assert "RunSmokeTestSupervisor.ps1" in smoke_batch
    assert "TimeoutSeconds 180" in smoke_batch
    assert "SMOKE_EXIT=%ERRORLEVEL%" in smoke_batch
    assert "RequestExitWithStatus" in smoke_module
    assert "Failed > 0 ? 1 : 0" in smoke_module
    assert "WaitForExit" in smoke_supervisor
    assert "taskkill.exe" in smoke_supervisor
    assert "Result: (\\d+) passed, (\\d+) failed" in smoke_supervisor
    assert "Requesting editor exit with status" in smoke_supervisor

    topology = read("RunRoleBattleDay7PackagedTopology.ps1")
    assert "Get-ChildItem -LiteralPath $AbilityDefinitionRoot" in topology
    assert "-Filter '*.xml'" in topology
    assert "AbilityDefinitionFiles" in topology

    for mode, runner in (("Listen", "RunRoleBattleDay7ListenSmoke.ps1"), ("Dedicated", "RunRoleBattleDay7DedicatedSmoke.ps1")):
        source = read(runner)
        assert f"-Mode {mode}" in source
        assert "Day7SpecificStatus" in source
        assert "Day6PrerequisiteStatus" in source
        assert "AggregatePassed" in source
        assert "powershell.exe" in source
        assert "RunRoleBattleDays789Automation.ps1" in source
        assert "RunRoleBattleDay6NetworkSmoke.ps1" in source

    day7_doc = read("Docs/Plans/Role-Battle-Implementation/Day-07-Combat-Roles.md")
    assert day7_doc.count("RunRoleBattleDay5ConfigSmoke.ps1' -Mode") == 2
    assert day7_doc.count("RunRoleBattleDay6NetworkSmoke.ps1' -Mode") == 2

    assert "SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding" in read("Source/Aura/Private/Game/AuraGameModeBase.cpp")

    for runner in (
        "RunRoleBattleDay2NetworkSmoke.ps1",
        "RunRoleBattleDay3NetworkSmoke.ps1",
        "RunRoleBattleDay4DamageSmoke.ps1",
        "RunRoleBattleDay5ConfigSmoke.ps1",
        "RunRoleBattleDay6NetworkSmoke.ps1",
        "RunRoleBattleDay7PackagedTopology.ps1",
        "RunRoleBattleCivilianNetworkSmoke.ps1",
        "RunRoleBattleDays1012NetworkSmoke.ps1",
        "RunRoleBattleDay16NetworkSmoke.ps1",
        "RunRoleBattleDay17NetworkSmoke.ps1",
    ):
        source = read(runner)
        assert "Guid]::NewGuid" in source or "[guid]::NewGuid" in source, runner

    xml_files = sorted(path.name for path in (ROOT / "Content" / "AbilityDefinitions").glob("*.xml"))
    assert len(xml_files) == 9, xml_files
    assert re.search(r"\$RequiredDefinitions\s*=\s*@\(Get-ChildItem", topology)
    print("Role/Battle review-fix contracts: PASS (smoke gate, Day 7 composition, XML discovery, respawn, and persistence isolation)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
