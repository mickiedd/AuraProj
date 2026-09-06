"""Focused source/data contract checks for the Day 7-9 role battle slice."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> int:
    role = json.loads(read("Content/Config/RoleConfig.json"))
    levels = json.loads(read("Content/Config/LevelConfig.json"))
    population = json.loads(read("Content/Config/PopulationSpawnTable.json"))
    profiles = json.loads(read("Content/Config/CivilianWorkProfiles.json"))
    ini = read("Config/DefaultGame.ini")
    civilian_cpp = read("Source/Aura/Private/Character/AuraCivilian.cpp")
    manager_cpp = read("Source/Aura/Private/World/AuraPopulationManager.cpp")
    types_h = read("Source/Aura/Public/World/AuraPopulationTypes.h")
    automation_cpp = read("Source/Aura/Private/Tests/AuraRoleBattleDays789Tests.cpp")
    role_automation_cpp = read("Source/Aura/Private/Tests/AuraRoleBattleTests.cpp")

    roles = {entry["role"]: entry for entry in role["roles"]}
    assert roles["Aura"]["lmbAbilityDefinition"].endswith("FireBolt.xml")
    assert "BungeeMan" not in roles
    assert roles["Civilian"]["entityType"] == "Entity.AmbientNPC"
    assert roles["Civilian"]["controlType"] == "Control.CivilianAI"
    assert roles["Civilian"]["playerSelectable"] is True
    assert roles["Civilian"]["canAttack"] is False
    assert "+DirectoriesToAlwaysStageAsUFS=(Path=\"AbilityDefinitions\")" in ini

    level_ids = {entry["id"] for entry in levels["levels"]}
    row = population["populations"][0]
    assert population["schemaVersion"] == 1
    assert row["mapId"] in level_ids
    assert row["actorClass"] == "/Script/Aura.AuraCivilian"
    assert row["initialCount"] == 3 and row["maximumCount"] == 5
    assert row["defaultWorkProfileId"] == "Observer"
    assert row["memberOverrides"] == [{"slotIndex": 1, "merchantDefinitionId": "MarketMerchant"}]
    assert profiles["schemaVersion"] == 1
    assert {profile["id"] for profile in profiles["workProfiles"]} == {"Observer"}

    for field in (
        "PopulationId",
        "PopulationSlotIndex",
        "PopulationMemberId",
        "WorkProfileId",
        "ZoneId",
        "MerchantDefinitionId",
    ):
        assert f"{field}" in types_h
    for contract in (
        "ApplyRoleAtSpawn",
        "SetPopulationMemberState",
        "SetRequestedCivilianRoleId",
        "AbilityActorInfoSet",
        "SetActorEnableCollision(false)",
    ):
        assert contract in civilian_cpp
    for contract in (
        "BuildDeterministicMemberId",
        "ReservedMemberIds.Remove(MemberId)",
        "bInitialPopulationFinalized",
        "IsChildOf(AAuraCivilian::StaticClass())",
        "Override.SlotIndex == SlotIndex",
    ):
        assert contract in manager_cpp

    expected_tests = {
        7: {
            "AuraDefinitionContract",
            "SameFactionRejection",
            "IsolatedSingleProfileSaveReloadAndRespawn",
            "ClientCannotGrantOrDamage",
        },
        8: {
            "CivilianRoleIdentity",
            "CivilianRoleMovementApplicationContract",
            "CivilianASCInitialization",
            "EmptyOffensiveLoadout",
            "InvalidRoleFailsClosed",
            "PresentationIdempotence",
            "DefaultDamageDeniedAndTrustedFixtureAllowed",
        },
        9: {
            "PopulationSchemaValidation",
            "WorkProfileValidation",
            "DeterministicMemberIds",
            "InvalidRoleClassRejected",
            "DuplicateInitialization",
            "SpawnFailureReleasesSlot",
            "PreStartPlayManagerAvailability",
            "MemberStateReplication",
            "PerMemberOverrideIsolation",
        },
    }
    for day, test_names in expected_tests.items():
        for test_name in test_names:
            assert f'"Day{day}.{test_name}"' in automation_cpp
    assert "FAuraDay5ExplicitFourRoleCatalogTest" in role_automation_cpp

    for runner in (
        "RunRoleBattleDay7ListenSmoke.ps1",
        "RunRoleBattleDay7DedicatedSmoke.ps1",
        "RunRoleBattleDay7PackagedSmoke.ps1",
        "RunRoleBattleDay8NetworkSmoke.ps1",
        "RunRoleBattleDay9NetworkSmoke.ps1",
    ):
        assert (ROOT / runner).is_file(), runner

    print("Day 7-9 role, Civilian, population, and packaging contracts: PASS (26 named tests and 5 topology runners present)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
