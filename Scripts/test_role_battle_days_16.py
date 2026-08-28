from pathlib import Path
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


currency = read("Source/Aura/Private/Economy/AuraCurrencyComponent.cpp")
inventory = read("Source/Aura/Private/Economy/AuraInventoryComponent.cpp")
player_state = read("Source/Aura/Private/Player/AuraPlayerState.cpp")
player_state_h = read("Source/Aura/Public/Player/AuraPlayerState.h")
character = read("Source/Aura/Private/Character/AuraCharacter.cpp")
economy_types = read("Source/Aura/Public/Economy/AuraEconomyTypes.h")

for token in ("COND_OwnerOnly", "Amount <= Maximum - Balance", "NotAuthority", "CommitCreditCurrency"):
    require(token in currency, f"Day 16 wallet contract missing: {token}")
for token in ("FastArrayDeltaSerialize", "MarkArrayDirty", "CanAddItem", "CanRemoveItem", "MoveTemp(Candidate)"):
    require(token in inventory or token in economy_types, f"Day 16 inventory contract missing: {token}")
for token in ("CreateDefaultSubobject<UAuraCurrencyComponent>", "CreateDefaultSubobject<UAuraInventoryComponent>", "NewEphemeralSession", "EconomyInitializationCount"):
    require(token in player_state, f"Day 16 PlayerState contract missing: {token}")
require("InitializeEconomyForNewProfileOnce" in character, "Day 16 initialization must happen before pawn role application")
require("UFUNCTION(Server" not in read("Source/Aura/Public/Economy/AuraCurrencyComponent.h"), "Wallet must not expose a mutation RPC")
require("UFUNCTION(Server" not in read("Source/Aura/Public/Economy/AuraInventoryComponent.h"), "Inventory must not expose a mutation RPC")
require("ReplicatedUsing = OnRep_EconomyInitializationState" in player_state_h, "Initialization state must replicate owner-only")
for config in ("EconomyConfig.json", "ItemDefinitions.json"):
    require(json.loads(read(f"Content/Config/{config}")).get("schemaVersion") == 1, f"{config} schemaVersion must be 1")
xml_root = ET.parse(ROOT / "Content/AutoTests/RoleBattleDay16OwnerReplication.xml").getroot()
require(xml_root.tag == "behavior" and xml_root.find("node") is not None, "Day 16 AutoTest XML must be runnable")
runner = read("RunRoleBattleDay16NetworkSmoke.ps1")
require("ValidateSet('Listen', 'Dedicated')" in runner and "$Mode" in runner, "Day 16 runner must declare both topologies")
require("-AuraPersistenceProvider=NULL" in runner, "Day 16 Null-OSS smoke must override the production persistence provider")
require("IsReadyForPersistentSave" in player_state_h and "IsReadyForPersistentSave" in read("Source/Aura/Private/Game/AuraGameModeBase.cpp"), "Logout must skip incomplete persistent profiles")
require('Day16Client1' in read("Source/Aura/Private/Game/AuraGameModeBase.cpp"), "Day 16 Listen probe must target the named external client rather than the host controller")
require('SetPendingPersistentProfile(nullptr)' in read("Source/Aura/Private/Character/AuraCharacter.cpp"), "Respawn must not re-apply the initial pending profile snapshot")

names = read("Source/Aura/Private/Tests/AuraEconomyStateTests.cpp")
require(names.count("Aura.RoleBattle.Day16.") == 8, "Expected 8 named Day 16 native tests")
print("Day 16 wallet, inventory, owner-only, and lifecycle contracts: PASS (8 named tests present)")
