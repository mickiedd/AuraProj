"""Focused source/data contracts for the Day 10-12 role-battle slice."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def main() -> int:
    zones = json.loads(read("Content/Config/BattleZones.json"))
    profiles = json.loads(read("Content/Config/CivilianWorkProfiles.json"))
    tests = read("Source/Aura/Private/Tests/AuraRoleBattleDays1012Tests.cpp")
    destination = read("Source/Aura/Private/AI/BTTask_FindCivilianDestination.cpp")
    threat = read("Source/Aura/Private/AI/BTService_FindNearestThreat.cpp")
    controller = read("Source/Aura/Private/AI/AuraCivilianAIController.cpp")
    state = read("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp")
    dispatcher = read("Source/Aura/Private/Combat/AuraDeathPolicyDispatcher.cpp")
    director = read("Source/Aura/Private/Battle/AuraBattleDirector.cpp")
    library = read("Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp")

    assert profiles["schemaVersion"] == 1
    profile = profiles["workProfiles"][0]
    assert profile["movement"]["moveTimeout"] > 0
    assert profile["movement"]["fleeSpeed"] > profile["movement"]["speed"]
    assert profile["threat"]["calmDuration"] >= 0

    assert zones["schemaVersion"] == 1 and len(zones["zones"]) >= 2
    assert any(zone["safeZone"] for zone in zones["zones"])
    assert all("policy" in zone and "priority" in zone for zone in zones["zones"])

    for needle in (
        "CanDamage(Candidate, Civilian",
        "AAuraCivilianShelterMarker",
        "TryReserveActivityMarker",
        "EAuraCivilianActivity::Flee",
    ):
        assert needle in destination or needle in threat
    assert "BTDecorator_CivilianAlive" in controller
    assert "StopLogic" in controller and "StopMovement" in controller
    assert "++DeathSequence" in state
    assert "FObjectKey" in dispatcher and "HighestDispatchedSequenceByVictim" in dispatcher
    assert "PointerHash" not in dispatcher
    assert "TargetActor->GetActorLocation()" in director
    assert "RuleContext = AuthoritativeContext" in library
    assert "ResolveCombatRuleContext(Candidate, Civilian" in controller
    assert destination.find("GetPathLength") < destination.find("TryReserveActivityMarker")

    expected = {
        "Day10": (
            "WorkProfileValidation", "ThreatQueryDirection", "ProtectedCandidateIgnored",
            "TrustedTestThreatPolicy", "UntargetableDamageSourceStillThreat", "MarkerReservationCapacity",
            "UnreachableShelterRecovery", "LifeStateStopsBrain", "NoAttackNodeOrGrant",
            "FindNearestHostileBlackboardContract", "EnemyTargetingRegression", "NoFindNearestPlayerReferences",
        ),
        "Day11": (
            "TransitionWinner", "RepeatedFatalDamage", "DeathPolicyDispatch", "EnemyRewardExactlyOnce",
            "CivilianNoReward", "NeutralDeathEventAttribution", "CombatRulesRejectNonAlive", "OnRepPresentationIdempotence",
        ),
        "Day12": (
            "BattleZoneSchemaValidation", "SafeZonePrecedence", "OverlappingSafeZonePriorityAndLexicalTie",
            "PriorityAndLexicalTie", "TargetLocationResolution", "UntrustedCallerContextOverwritten",
            "ProjectileBoundaryReevaluation", "BattleAttributionNetSerialize", "DeathEventCarriesBattleIds",
            "ValidPhaseTransitions", "EventIdLifecycle",
        ),
    }
    for day, names in expected.items():
        for name in names:
            assert f'"{day}.{name}"' in tests, f"missing native test {day}.{name}"

    for runner in (
        "RunRoleBattleDay10NetworkSmoke.ps1",
        "RunRoleBattleDay11NetworkSmoke.ps1",
        "RunRoleBattleDay12NetworkSmoke.ps1",
    ):
        assert (ROOT / runner).is_file(), runner

    print("Day 10-12 civilian AI, death policy, and battle director contracts: PASS (31 named tests present)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
