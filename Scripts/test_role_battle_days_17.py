from pathlib import Path
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


merchant = read("Source/Aura/Private/Economy/AuraMerchantComponent.cpp")
merchant_h = read("Source/Aura/Public/Economy/AuraMerchantComponent.h")
commerce = read("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp")
commerce_h = read("Source/Aura/Public/Economy/AuraCommerceSubsystem.h")
interaction = read("Source/Aura/Private/Interaction/AuraInteractionComponent.cpp")
interaction_h = read("Source/Aura/Public/Interaction/AuraInteractionComponent.h")
civilian = read("Source/Aura/Private/Character/AuraCivilian.cpp")
policy = read("Source/Aura/Private/Interaction/AuraInteractionPolicy.cpp")
controller = read("Source/Aura/Private/UI/WidgetController/MerchantWidgetController.cpp")
widget = read("Source/Aura/Private/UI/Widget/AuraMerchantWidget.cpp")

for token in ("DOREPLIFETIME(UAuraMerchantComponent, Presentation)", "ApplyAuthorityPresentation", "SetAuthorityUnavailable"):
    require(token in merchant or token in merchant_h, f"merchant component contract missing: {token}")
for token in ("PopulationMemberId", "MerchantDefinitionId", "OfferIds", "CurrentStock", "StockRevision"):
    require(token in commerce, f"commerce runtime contract missing: {token}")
for token in ("ReplayCache", "ReplayOrder", "LastAcceptedRequestId", "RequestGap", "MAX_uint64", "256"):
    require(token in commerce, f"replay contract missing: {token}")
for token in ("CommitDebitCurrency(CurrencyIdBefore, Offer.BuyPrice, false)", "CommitAddItem(Offer.ItemId, Offer.GrantQuantity, false)", "RestoreCurrencyState(CurrencyIdBefore", "RestoreInventoryState(InventoryBefore", "SavePlayerAndWorldCheckpoint", "PersistenceFailed", "bPersistenceRequired", "NM_Standalone"):
    require(token in commerce, f"atomic transaction contract missing: {token}")
for token in ("UFUNCTION(Server, Reliable)", "ServerRequestPurchase", "FGuid SessionNonce", "uint64 RequestId", "FName OfferId"):
    require(token in interaction_h, f"owned purchase RPC contract missing: {token}")
require("UAuraCommerceSubsystem" in interaction and "ProcessPurchase" in interaction, "interaction endpoint must delegate to commerce subsystem")
require("MerchantComponent = CreateDefaultSubobject<UAuraMerchantComponent>" in civilian, "Civilian must own stable merchant component")
require("OnLifeStateChanged" in civilian and "MarkMerchantUnavailable" in civilian, "Civilian death must close merchant presentation immediately")
require("Interaction_Trade" in policy and "HandlerId = TEXT(\"Trade\")" in policy, "Trade policy must be server-resolved")
require("OnPurchaseResult" in controller and "RequestPurchase" in controller and "BuyOffer" in widget, "merchant UI/controller flow missing")

names = read("Source/Aura/Private/Tests/AuraCommerceTests.cpp")
require(names.count("Aura.RoleBattle.Day17.") == 13, "Expected 13 named Day 17 native tests")
xml_root = ET.parse(ROOT / "Content/AutoTests/RoleBattleDay17Merchant.xml").getroot()
require(xml_root.tag == "behavior" and xml_root.find("node") is not None, "Day 17 AutoTest XML must be runnable")
for path in (
    "Content/Blueprints/UI/Merchant/WBP_Merchant.snapshot.json",
    "Content/Blueprints/UI/Merchant/WBP_MerchantOfferRow.snapshot.json",
    "Content/Blueprints/UI/WidgetController/BP_MerchantWidgetController.snapshot.json",
    "Content/Blueprints/UI/HUD/BP_AuraHUD.snapshot.json",
):
    json.loads(read(path))
runner = read("RunRoleBattleDay17NetworkSmoke.ps1")
require("ValidateSet('Listen', 'Dedicated')" in runner and "-RoleBattleDay17NetworkProbe" in runner, "Day 17 runner must declare both topologies and the probe")
require("-AuraPersistenceProvider=NULL" in runner, "Day 17 Null-OSS smoke must override the production persistence provider")

print("Day 17 merchant, replay, atomicity, and UI contracts: PASS (13 named tests present)")
