from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


manager = text("Source/Aura/Private/World/AuraPopulationManager.cpp")
interaction = text("Source/Aura/Private/Interaction/AuraInteractionComponent.cpp")
policy = text("Source/Aura/Private/Interaction/AuraInteractionPolicy.cpp")
player_controller = text("Source/Aura/Private/Player/AuraPlayerController.cpp")
game_mode = text("Source/Aura/Private/Game/AuraGameModeBase.cpp")
population_header = text("Source/Aura/Public/World/AuraPopulationManager.h")
economy = text("Source/Aura/Private/Economy/AuraEconomyConfig.cpp")
tests = text("Source/Aura/Private/Tests/AuraRoleBattleDays1315Tests.cpp")

for token in (
    "AuthoritativeDeath", "AuthoritativeDeadState", "CorpseCleaned", "DeterministicRefill",
    "PhaseChangedDisallowed", "PhaseDisallowedAtExecution", "Shutdown", "BuildDebugSnapshot",
):
    require(token in manager, f"Day 13 lifecycle token missing: {token}")
require("OnSpawnedMonsterDestroyed" not in manager, "Civilian manager must not own Enemy respawn")

for token in (
    "ServerRequestInteraction", "LastAcceptedRequestId", "OutOfRange", "LineOfSightBlocked",
    "FAuraInteractionPolicy::Resolve", "FeatureUnavailable", "ExecuteAuraInteraction",
):
    require(token in interaction or token in policy, f"Day 14 authority guard missing: {token}")

for token in (
    "ParseCanonicalNonNegativeInt64", "int64 overflow", "Atomic reload semantics", "allowedWorkProfileIds",
    "supportedCurrencyId", "duplicates merchantDefinitionId", "TryGetString", "TryLoad",
):
    require(token in economy or token in text("Source/Aura/Private/Economy/AuraEconomyRegistrySubsystem.cpp"),
            f"Day 15 registry guard missing: {token}")

for token in ("InteractionProfileTag", "GetAuraTargetZoneId", "ZoneDenied"):
    require(token in policy, f"Day 14 policy authorization guard missing: {token}")
for token in ("!CursorHit.bBlockingHit", "TargetInteractionWidgetController", "SelectedInteractionOptionIndex"):
    require(token in player_controller, f"Day 14 focus/input guard missing: {token}")
for token in ("WorldReadiness != EAuraWorldReadiness::Ready", "bEconomyRegistryLoadedForCurrentWorld"):
    require(token in game_mode, f"Day 15 startup guard missing: {token}")
require("TryReadStrictString" in economy and "Type == EJson::String" in economy, "Day 15 parser must reject non-string IDs and values")
require("RecordedPopulationDeathCount" in population_header, "Day 13 repeated-death metric must be a counter")

for config in ("ItemDefinitions.json", "MerchantDefinitions.json", "EconomyConfig.json"):
    data = json.loads(text(f"Content/Config/{config}"))
    require(data.get("schemaVersion") == 1, f"{config} schemaVersion must be 1")

require(tests.count("Aura.RoleBattle.Day13.") == 10, "Expected 10 named Day 13 tests")
require(tests.count("Aura.RoleBattle.Day14.") == 11, "Expected 11 named Day 14 tests")
require(tests.count("Aura.RoleBattle.Day15.") == 7, "Expected 7 named Day 15 tests")

print("Day 13-15 population lifecycle, interaction, and economy contracts: PASS (28 named tests present)")
