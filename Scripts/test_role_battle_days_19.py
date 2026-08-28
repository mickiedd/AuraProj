from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str) -> None:
    assert (ROOT / relative).exists(), relative


def contains(relative: str, *tokens: str) -> bool:
    source = read(relative)
    return all(token in source for token in tokens)


def test_named_native_matrix_is_complete() -> None:
    source = read("Source/Aura/Private/Tests/AuraRoleBattleMultiplayerTests.cpp")
    names = [
        "ServerAuthorityBoundary", "RoleSelectionServerOwned", "AbilityActivationRequiresEquippedSlottedSpec",
        "DamageRulesRemainAuthoritative", "FireGun.AuthorityConfigured", "FireGun.CooldownConfigured",
        "FireGun.AttributionReplicates", "FriendlyFireDenied", "ProtectedCivilianDenied",
        "CommerceAuthority", "ForgedCommercePayloadRejected", "ReplayCacheBounded",
        "OwnerOnlyEconomy", "LateJoinUsesReplicatedState", "ReconnectRotatesSessionNonce",
        "ExactlyOnceDeath", "PopulationStableIds", "MerchantDeathClosure",
        "PlayerOwnedInteractionRoute", "WorldAndProfileIsolation", "ListenAndDedicatedRequired",
        "DevelopmentProbeOnly", "EmulationProfileRecorded", "BoundedFixture", "NoShippingMutationPath",
    ]
    assert all(name in source for name in names)


def test_network_probe_has_server_only_snapshot() -> None:
    require("Source/Aura/Public/Tests/AuraRoleBattleNetworkProbe.h")
    require("Source/Aura/Private/Tests/AuraRoleBattleNetworkProbe.cpp")
    assert contains("Source/Aura/Public/Tests/AuraRoleBattleNetworkProbe.h", "ReplayCacheLimit = 256", "HasRequiredFixture")
    assert contains("Source/Aura/Private/Tests/AuraRoleBattleNetworkProbe.cpp", "CollectServerSnapshot", "NM_Client", "GetNetConnection")


def test_runner_requires_both_topologies_and_prerequisite() -> None:
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    assert all(token in runner for token in [
        "ValidateSet('Listen', 'Dedicated')", "RunRoleBattleDay18PersistenceSmoke.ps1",
        "-Mode", "AuraServer.exe", "Day18ReconnectPrerequisite", "RoleBattleDay19MultiplayerProbe",
        "RoleBattleDay19MultiplayerProbeClientA", "RoleBattleDay19MultiplayerProbeClientB",
    ])


def test_runner_enforces_security_matrix() -> None:
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    assert all(token in runner for token in [
        "SecurityMatrix", "ExactlyOnceContention", "ReplayGapAndNonceRejection",
        "OwnerPrivacyAndNoClientMutation", "LateJoinReplication", "request=3 result=4",
        "request=4 result=1", "replay=1", "ForeignEconomyState",
    ])


def test_runner_records_network_emulation() -> None:
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    assert all(token in runner for token in [
        "NetworkLagMs = 100", "NetworkJitterMs = 20", "PacketLossPercent = 2",
        "-PktLag=", "-PktLagVariance=", "-PktLoss=", "NetworkEmulation =",
    ])


def test_runner_enforces_performance_thresholds() -> None:
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    assert all(token in runner for token in [
        "WarmupSeconds = 30", "SampleSeconds = 60", "Get-Percentile",
        "-gt 33.3", "-gt 50.0", "-gt 128.0", "-gt 512", "PerformanceBounded",
    ])


def test_authority_hardening_is_explicit() -> None:
    source = read("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp")
    assert all(token in source for token in [
        "ServerUpgradeAttribute_Implementation", "IsOwnerActorAuthoritative",
        "Attributes_Primary_Strength", "GetAttributePoints",
        "ServerSpendSpellPoint_Implementation", "GetSpellPoints",
        "ServerRequestActivateAbility_Implementation", "FindAbilitySpecFromHandle",
        "ServerEquipAbility_Implementation", "bPassiveSlot",
    ])


def test_combat_and_damage_paths_are_shared() -> None:
    for relative in [
        "Source/Aura/Private/Combat/AuraCombatRules.cpp",
        "Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp",
        "Source/Aura/Private/Actor/AuraProjectile.cpp",
    ]:
        require(relative)
    assert contains("Source/Aura/Private/Combat/AuraCombatRules.cpp", "CanDamage", "CanReceiveDamage", "bTargetProtected")
    assert contains("Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp", "GetSourceAbilitySystemComponent", "GetTargetAbilitySystemComponent")
    assert contains("Source/Aura/Private/Actor/AuraProjectile.cpp", "HasAuthority()", "ApplyDamageEffect")


def test_firegun_data_and_replication_contract() -> None:
    assert contains("Content/Config/RoleConfig.json", "FireGun.xml")
    assert contains("Content/AbilityDefinitions/FireGun.xml", "InputTag.LMB", "Cooldown.Gun.Fire")
    assert contains("Content/Config/ProjectileDefinitions.json", "fireGunBullet")
    assert contains("Source/Aura/Private/Character/AuraCharacterBase.cpp", "MulticastPlayGunFireFX")


def test_privacy_and_reconnect_contract() -> None:
    assert contains("Source/Aura/Private/Player/AuraPlayerState.cpp", "COND_OwnerOnly", "ApplyPersistentProfile")
    assert contains("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp", "RotateSession", "ReplayCache.Reset", "while (Session.ReplayOrder.Num() > 256)")
    assert contains("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp", "WorldPersistenceId", "IdentityKey", "BuildSlotName")


def test_lifecycle_contract() -> None:
    assert contains("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp", "TryEnterDying", "DeathSequence", "TryEnterDead")
    assert contains("Source/Aura/Private/World/AuraPopulationManager.cpp", "PopulationMemberId", "RestoreDormantSlot", "bPersistenceSnapshotConfigured")
    assert contains("Source/Aura/Private/Character/AuraCivilian.cpp", "HandleMerchantLifeStateChanged", "SetAuthorityUnavailable")


def test_xml_contract() -> None:
    root = ET.parse(ROOT / "Content/AutoTests/RoleBattleDay19VerticalSlice.xml").getroot()
    assert root.attrib["agenttype"] == "AuraAutoTest"
    assert {node.attrib["name"] for node in root.findall(".//node")} >= {
        "AuthorityAndRoleSecurity", "FireGunAuthorityCooldownAttribution", "CommerceForgeryReplayAndPrivacy",
        "LateJoinReconnectAndNonce", "ListenAndDedicated", "PerformanceAndBoundedState",
    }


def test_no_fake_map_or_shipping_probe_path() -> None:
    assert not (ROOT / "Content/Maps/Tests/RoleBattleDay19.umap").exists()
    assert contains("Source/Aura/Private/Game/AuraGameModeBase.cpp", "#if !UE_BUILD_SHIPPING", "RoleBattleDay19NetworkProbe")
    assert contains(
        "Source/Aura/Private/Player/AuraPlayerController.cpp",
        "#if !UE_BUILD_SHIPPING",
        "RoleBattleDay19MultiplayerProbeClientA",
        "IsBroomMountedByControlledCharacter",
        "FMath::IsFinite(WorldDirection.X)",
        "FMath::Clamp(ScaleValue, 0.f, 1.f)",
    )
    mount_component = read("Source/Aura/Private/Vehicle/AuraBroomMountComponent.cpp")
    mount_header = read("Source/Aura/Public/Vehicle/AuraBroomMountComponent.h")
    assert "ServerRequestMount" not in mount_component and "ServerRequestDismount" not in mount_component
    assert "ServerRequestMount" not in mount_header and "ServerRequestDismount" not in mount_header
    assert contains(
        "Source/Aura/Private/Vehicle/AuraBroomMountComponent.cpp",
        "AuraPC->RequestBroomMount(GetBroomOwner())",
        "AuraPC->RequestBroomDismount(GetBroomOwner())",
    )


def test_deep_review_fixes_are_regression_covered() -> None:
    game_mode = read("Source/Aura/Private/Game/AuraGameModeBase.cpp")
    commerce = read("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp")
    inventory_tests = read("Source/Aura/Private/Tests/AuraRoleBattleTests.cpp")
    runner = read("RunRoleBattleDay19Multiplayer.ps1")
    config = read("Config/DefaultGame.ini")

    assert all(token in game_mode for token in [
        "PositionAndStabilizeFixturePlayers", "DisableMovement()", "FVector(40.f, 20.f",
        "FVector(80.f, 40.f", "FVector(100.f, 60.f",
    ])
    assert all(token in commerce for token in ["TActorIterator<APawn>", "AddIgnoredActor", "EngineUtils.h"])
    assert "AuraRoleBattleMultiplayerTests.cpp" in inventory_tests
    assert all(token in runner for token in [
        "CookedStartupMap", "StartupMap.umap", "Saved\\Cooked\\WindowsServer\\Aura\\Saved\\Logs",
    ])
    assert '+MapsToCook=(FilePath="/Game/Maps/StartupMap")' in config


def main() -> None:
    tests = [value for name, value in globals().items() if name.startswith("test_") and callable(value)]
    for test in tests:
        test()
    print(f"Day 19 multiplayer hardening contracts: PASS ({len(tests)} checks)")


if __name__ == "__main__":
    main()
