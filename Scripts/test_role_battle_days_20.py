from pathlib import Path
import json
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str) -> None:
    assert (ROOT / relative).exists(), relative


def contains(relative: str, *tokens: str) -> bool:
    source = read(relative)
    return all(token in source for token in tokens)


def test_day20_native_matrix_is_complete() -> None:
    source = read("Source/Aura/Private/Tests/AuraRoleBattleDay20Tests.cpp")
    names = [
        "Cleanup.SharedCombatRules", "Cleanup.PickupEligibilityPolicy",
        "FireGun.SingleActivePath", "FireGun.AssetReferences",
        "Security.ShippingMutationSurface", "Roles.IdempotentGrantLedger",
        "Civilian.IndependentLifecycle", "Economy.OwnerPurchaseOnly",
        "Persistence.LifecycleCleanup", "Respawn.CollisionSafeFallback",
        "MerchantProbe.NamedJoinBarrier", "Cleanup.NoLegacyNearestPlayerService",
        "Cleanup.LegacyBlueprintSnapshotInactive",
        "Schemas.ReproducibleProcedure", "Limitations.Documented",
    ]
    assert all(name in source for name in names)


def test_shared_policy_has_no_legacy_direct_faction_checks() -> None:
    assert contains("Source/Aura/Private/Actor/AuraEffectActor.cpp", "FAuraPickupEligibility::CanReceive")
    assert contains("Source/Aura/Private/Combat/AuraPickupEligibility.cpp", "Faction_Enemy", "bAllowEnemies")
    assert contains("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp", "FAuraCombatRules::CanDamage")
    effect_actor = read("Source/Aura/Private/Actor/AuraEffectActor.cpp")
    melee = read("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp")
    assert 'ActorHasTag(FName("Enemy"))' not in effect_actor
    assert "IsNotFriend(" not in melee


def test_firegun_active_path_and_compatibility_are_explicit() -> None:
    role_config = read("Content/Config/RoleConfig.json")
    assert '"lmbAbilityDefinition": "/Game/AbilityDefinitions/FireGun.xml"' in role_config
    assert "AuraFireGun" not in role_config
    assert contains("Source/Aura/Public/AbilitySystem/Abilities/AuraFireGun.h", "non-active compatibility", "not granted or selected by RoleConfig")
    assert contains("Content/AbilityDefinitions/FireGun.xml", "Abilities.Gun.Fire", "Event.Montage.FireGun")
    require("Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/GA_FireGun.uasset")
    assert contains("Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/GA_FireGun.snapshot.json", "/Script/Aura.AuraFireGun")


def test_firegun_projectile_presentation_contract_is_complete() -> None:
    projectile_config = json.loads(read("Content/Config/ProjectileDefinitions.json"))["projectiles"]["fireGunBullet"]
    assert projectile_config["nativeClass"] == "/Script/Aura.AuraBullet"
    assert projectile_config["tracerMesh"] == "/Game/MilitaryWeapDark/FX/Meshes/St_Tracer_A.St_Tracer_A"
    assert projectile_config["flightParticle"] == "/Game/MilitaryWeapDark/FX/P_AssaultRifle_Tracer_01.P_AssaultRifle_Tracer_01"
    assert projectile_config["impactParticle"] == "/Game/MilitaryWeapDark/FX/P_Impact_Stone_Medium_01.P_Impact_Stone_Medium_01"
    assert projectile_config["impactSound"] == "/Game/MilitaryWeapDark/Sound/Rifle/Rifle_ImpactSurface_Cue.Rifle_ImpactSurface_Cue"
    assert projectile_config["surfaceMarkMaterial"] == "/Game/Assets/Effects/Combat/M_BulletHoleClean.M_BulletHoleClean"
    assert projectile_config["surfaceMarkSize"] > 0 and projectile_config["surfaceMarkLifeSpan"] > 0
    require("Content/Assets/Effects/Combat/M_BulletHoleClean.uasset")

    bullet_source = read("Source/Aura/Private/Actor/AuraBullet.cpp")
    projectile_source = read("Source/Aura/Private/Actor/AuraProjectile.cpp")
    assert all(token in bullet_source for token in [
        "Definition.TracerMesh.TryLoad()", "Definition.FlightParticle.TryLoad()",
        "Definition.ImpactParticle.TryLoad()", "SpawnEmitterAttached",
        "SpawnDecalAtLocation", "ImpactNormal",
    ])
    assert all(token in projectile_source for token in [
        "FVector_NetQuantizeNormal", "Hit.ImpactPoint", "Hit.ImpactNormal",
        "ApplySurfaceImpactAndDestroy",
    ])


def test_shipping_mutation_surface_is_removed_or_guarded() -> None:
    controller = read("Source/Aura/Private/Player/AuraPlayerController.cpp")
    header = read("Source/Aura/Public/Player/AuraPlayerController.h")
    assert not (ROOT / "Source/Aura/Public/Player/AuraCheatManager.h").exists()
    assert not (ROOT / "Source/Aura/Private/Player/AuraCheatManager.cpp").exists()
    assert "CheatClass" not in controller
    assert "ServerFullAbilities" not in controller and "RequestAddMonster" not in controller
    assert "DevelopmentOnly" in header and "AutoTestUseRandomEquippedAbility" in header
    assert "UE_BUILD_SHIPPING" in controller
    assert '"Name": "AuraAutoTestRuntime"' in read("Plugins/AuraAutoTest/AuraAutoTestPlugin.uplugin")
    assert '"Type": "DeveloperTool"' in read("Plugins/AuraAutoTest/AuraAutoTestPlugin.uplugin")


def test_role_ledger_and_lifecycle_cleanup_are_present() -> None:
    asc = read("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp")
    persistence = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    controller = read("Source/Aura/Private/Player/AuraPlayerController.cpp")
    assert all(token in asc for token in ["RoleGrantLedger.bInitialized", "RoleGrantLedger.bReconciled", "AddUnique", "GrantedRoleBySpecHandle"])
    assert "GrantAndEquipAllAbilities" not in asc
    assert all(token in persistence for token in ["void UAuraPersistenceSubsystem::Deinitialize", "PreparedProfiles.Reset", "ActiveProfiles.Reset"])
    assert all(token in controller for token in ["EndPlay", "InvalidateSession"])


def test_no_legacy_nearest_player_service() -> None:
    assert not (ROOT / "Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp").exists()
    assert not (ROOT / "Source/Aura/Public/AI/BTService_FindNearestPlayer.h").exists()


def test_legacy_blueprint_snapshot_is_not_an_active_grant_path() -> None:
    enemy_config = read("Content/Config/EnemyAbilityConfig.json")
    assert "EnemyMeleeAttack.xml" in enemy_config
    assert "GA_MeleeAttack" not in enemy_config
    assert contains("Content/AbilityDefinitions/EnemyMeleeAttack.xml", "EnemyMeleeDamage")


def test_release_xml_and_docs_are_parseable() -> None:
    release = ET.parse(ROOT / "Content/AutoTests/RoleBattleDay20Release.xml").getroot()
    assert release.attrib == {"version": "1", "agenttype": "AuraAutoTest"}
    assert {node.attrib["name"] for node in release.findall(".//node")} >= {
        "SharedCombatAndPickupPolicy", "FireGunSingleActiveDefinition", "ShippingMutationSurface",
        "PersistenceAndLifecycleCleanup", "StagedDataManifest", "ProductionAuthentication",
        "PackagedListenAndDedicated", "Pass",
    }
    assert contains("Docs/Reference/Role-Battle-Economy-Schema.md", "schema version 1", "IdentityProvider", "WorldPersistenceId")
    assert contains("Docs/Reference/Role-Battle-Vertical-Slice-Test-Procedure.md", "Listen", "Dedicated", "Production authentication")
    assert contains("Docs/README.md", "Role-Battle-Economy-Schema.md", "Role-Battle-Vertical-Slice-Test-Procedure.md")


def test_release_staging_and_all_json_contracts() -> None:
    config = read("Config/DefaultGame.ini")
    assert all(token in config for token in [
        '+MapsToCook=(FilePath="/Game/Maps/StartupMap")',
        '+DirectoriesToAlwaysStageAsUFS=(Path="Config")',
        '+DirectoriesToAlwaysStageAsUFS=(Path="AbilityDefinitions")',
        '+DirectoriesToAlwaysStageAsUFS=(Path="BehaviorTrees")',
    ])
    for path in sorted((ROOT / "Content/Config").glob("*.json")):
        json.loads(path.read_text(encoding="utf-8"))


def test_plugin_smoke_and_autotest_commands_remain_release_gates() -> None:
    plugin = read("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AuraAbilityGraphModule.cpp")
    runner = read("Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Private/AutoTestRunnerSubsystem.cpp")
    assert all(token in plugin for token in ["Run(TEXT(\"FireGunMigration\"", "Run(TEXT(\"RoleDefinitionLoading\""])
    assert all(token in runner for token in ["AutoTest.Run", "AutoTest.RunAll", "AutoTest.Stop"])


def test_day19_runner_still_requires_cooked_dedicated_map() -> None:
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    assert all(token in runner for token in ["CookedStartupMap", "StartupMap.umap", "Day18ReconnectPrerequisite"])
    assert not (ROOT / "Content/Maps/Tests/RoleBattleDay19.umap").exists()


def test_packaged_topology_supplies_world_health_contract() -> None:
    runner = read("RunRoleBattleDay7PackagedTopology.ps1")
    assert '$WorldPersistenceId = "RoleBattleDay7Packaged-$RunId"' in runner
    assert '"-WorldPersistenceId=$WorldPersistenceId"' in runner


def test_legacy_network_runners_supply_world_health_contract() -> None:
    world_id_runners = [
        "RunRoleBattleDay2NetworkSmoke.ps1", "RunRoleBattleDay3NetworkSmoke.ps1",
        "RunRoleBattleDay4DamageSmoke.ps1", "RunRoleBattleDay5ConfigSmoke.ps1",
        "RunRoleBattleDay6NetworkSmoke.ps1", "RunRoleBattleCivilianNetworkSmoke.ps1",
        "RunRoleBattleDays1012NetworkSmoke.ps1", "RunRoleBattleDay16NetworkSmoke.ps1",
        "RunRoleBattleDay17NetworkSmoke.ps1",
    ]
    for runner in world_id_runners:
        source = read(runner)
        assert "WorldPersistenceId" in source, runner
    assert "FindNearestHostile" in read("RunRoleBattleDay3NetworkSmoke.ps1")
    assert "Day17Client" in read("Source/Aura/Private/Game/AuraGameModeBase.cpp")


def test_day20_followups_are_explicit() -> None:
    todos = read("Docs/Tracking/GAS-Migration-TODOs.md")
    assert all(token in todos for token in ["Day 20 closeout", "ammunition", "hot swapping", "offline timer"])


def main() -> None:
    tests = [value for name, value in globals().items() if name.startswith("test_") and callable(value)]
    for test in tests:
        test()
    print(f"Day 20 cleanup/release contracts: PASS ({len(tests)} checks)")


if __name__ == "__main__":
    main()
