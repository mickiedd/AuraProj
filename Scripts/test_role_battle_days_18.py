from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


def test_game_server_launchers_make_development_provider_explicit() -> None:
    for launcher_name in ("StartGameServer.bat", "StartGameServer.command"):
        launcher = (ROOT / launcher_name).read_text(encoding="utf-8")
        assert "--persistence-provider" in launcher
        assert "AURA_PERSISTENCE_PROVIDER" in launcher
        assert "NULL" in launcher
        if launcher_name.endswith(".command"):
            assert "eval " not in launcher


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def read_bytes(relative: str) -> bytes:
    return (ROOT / relative).read_bytes()


def require(path: str) -> None:
    assert (ROOT / path).exists(), path


def test_named_native_suite_is_complete() -> None:
    source = read("Source/Aura/Private/Tests/AuraPersistenceTests.cpp")
    names = [
        "LegacyV0Migration", "FutureVersionRejected", "Int64RoundTrip", "PlayerIsolation",
        "LoadBeforePawnInitialization", "WorldLoadsOnce", "PopulationReconciliation",
        "MerchantStockRoundTrip", "CommittedTransactionOnly", "TornCheckpointRecovery",
        "CorruptNewestManifestFallsBack", "AuthenticatedIdentityRequired",
        "ProviderMismatchRejected", "DuplicateProfileConnectionRejected", "WorldPersistenceIdIsolation",
    ]
    assert all(f'Aura.RoleBattle.Day18.Save.{name}' in source for name in names)


def test_versioned_records_and_manifest_contract() -> None:
    for path in [
        "Source/Aura/Public/Game/AuraPlayerSaveGame.h",
        "Source/Aura/Public/Game/AuraWorldSaveGame.h",
        "Source/Aura/Public/Game/AuraPersistenceManifestSaveGame.h",
        "Source/Aura/Public/Game/AuraPersistenceSubsystem.h",
    ]:
        require(path)
    source = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    assert all(token in source for token in [
        "CurrentSchemaVersion", "ComputePlayerSaveChecksum", "ComputeWorldSaveChecksum",
        "ComputeManifestChecksum", "WriteManifest", "BuildManifestSlot", "RecordGeneration",
        "trying the prior manifest", "All available player generations were invalid",
        "SavePlayerAndWorldCheckpoint", "no committed role", "RoleBattleDay18PersistenceSmoke",
    ])


def test_identity_is_authenticated_and_hashed() -> None:
    source = read("Source/Aura/Private/Game/AuraPlayerProfileIdentity.cpp")
    assert all(token in source for token in [
        "FromUniqueNetId", "GetType()", "IsValid()", "BuildSlotName", "FCrc::StrCrc32",
        "OnlineSubsystemNull identities are local test identities",
    ])
    assert "PlayerName" not in source and "LoadSlotName" not in source


def test_authority_load_order_and_logout_binding() -> None:
    source = read("Source/Aura/Private/Game/AuraGameModeBase.cpp")
    population = read("Source/Aura/Private/World/AuraPopulationManager.cpp")
    character = read("Source/Aura/Private/Character/AuraCharacter.cpp")
    assert all(token in source for token in [
        "ConfigureAuthorityWorld", "ResolveConnectionIdentity", "PreparePlayerProfile",
        "LoadWorldState", "SaveWorldState", "void AAuraGameModeBase::Logout",
    ])
    assert "ApplyPersistenceSnapshot" in source and "ApplyPersistenceSnapshot" in population
    assert "AuraGameMode->LoadWorldState(GetWorld())" not in character
    assert "ApplyPersistentProfile" in character


def test_player_profile_is_before_pawn_grants() -> None:
    source = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    character = read("Source/Aura/Private/Character/AuraCharacter.cpp")
    assert "LoadBeforePawnInitialization=1" in source
    assert character.index("ApplyPersistentProfile") < character.index("ApplyRoleAtSpawn")


def test_world_snapshot_is_sorted_and_once_only() -> None:
    source = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    assert all(token in source for token in [
        "bWorldRestoreAttempted", "LoadedOnce=1", "PopulationSlots.Sort", "Merchants.Sort",
        "No valid manifest referenced", "WorldPersistenceId",
    ])


def test_merchant_snapshot_round_trip_contract() -> None:
    source = read("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp")
    assert all(token in source for token in [
        "CapturePersistenceState", "RestorePersistenceState", "StockRevision",
        "Saved merchant offer stock is invalid", "PublishMerchantPresentation",
    ])


def test_legacy_fixture_is_version_zero() -> None:
    fixture = read_bytes("Source/Aura/Private/Tests/Fixtures/RoleBattleLegacyV0.sav")
    assert fixture[:4] == b"GVAS"
    assert len(fixture) > 256
    source = read("Source/Aura/Private/Tests/AuraPersistenceTests.cpp")
    assert "LoadGameFromMemory" in source
    assert "Abilities.Gun.Fire" in source


def test_provider_preflight_is_explicit() -> None:
    engine = read("Config/DefaultEngine.ini")
    connection = read("Content/Config/ServerConnection.json")
    persistence = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    assert "ExpectedProviderName=Steam" in engine
    assert "WorldPersistenceId=AuraCampaignMain" in engine
    assert "worldPersistenceId" in connection
    assert "ExpectedProviderName\"), ExpectedProviderName, GEngineIni" in persistence
    assert "WorldPersistenceId\"), WorldPersistenceId, GEngineIni" in persistence


def test_xml_fixture_is_valid() -> None:
    root = ET.parse(ROOT / "Content/AutoTests/RoleBattleDay18Persistence.xml").getroot()
    assert root.attrib["agenttype"] == "AuraAutoTest"
    assert {node.attrib["name"] for node in root.findall(".//node")} >= {
        "AuthenticatedProfileIsolation", "WorldManifestRecovery", "LoadBeforePawnInitialization",
    }


def test_runner_has_two_modes_and_reconnect_assertions() -> None:
    runner = read("RunRoleBattleDay18PersistenceSmoke.ps1")
    assert "ValidateSet('Listen', 'Dedicated')" in runner
    assert all(token in runner for token in [
        "AuraFixtureIdentity=Day18ProfileA", "AuraFixtureIdentity=Day18ProfileB",
        "Reconnect", "WorldPersistenceId", "ProviderMismatch", "DuplicateProfile",
        "Passed", "NoCrash",
    ])
    assert "-AuraPersistenceProvider=NULL" in runner


def test_persistent_checkpoint_is_single_manifest_commit() -> None:
    source = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    assert source.index("SavePlayerProfileRecord") < source.index("SaveWorldStateRecord") < source.index("WriteManifest(WorldGeneration")
    assert source.index("ValidatePlayerSaveRecord(*Save") < source.index("SaveGameToSlot(Save")


def test_dormant_merchants_restore_after_population_respawn() -> None:
    commerce = read("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp")
    population = read("Source/Aura/Private/World/AuraPopulationManager.cpp")
    assert "PendingMerchantRestores" in commerce
    assert "IsDormantMerchantMember" in commerce and "IsDormantMerchantMember" in population
    assert "CapturePersistenceState" in commerce and "PendingMerchantRestores" in commerce


def test_no_client_profile_or_world_path_authority() -> None:
    source = read("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp")
    assert "Client chooses" not in source
    assert "Client" not in source.split("bool UAuraPersistenceSubsystem::PreparePlayerProfile", 1)[1].split("bool UAuraPersistenceSubsystem::ApplyPreparedProfile", 1)[0]
    assert "UGameplayStatics::SaveGameToSlot" in source


def main() -> None:
    tests = [value for name, value in globals().items() if name.startswith("test_") and callable(value)]
    for test in tests:
        test()
    print(f"Day 18 persistence contracts: PASS ({len(tests)} checks)")


if __name__ == "__main__":
    main()
