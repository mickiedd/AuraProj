// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/AuraSupplyRiskTypes.h"

namespace AuraGameplayDay52TestsPrivate
{
	FAuraRunSupplyMember Member(FName Id, FName Role, float Health = 1.0f, float Mana = 1.0f, int32 Ammo = 48)
	{
		FAuraRunSupplyMember Result;
		Result.MemberId = Id; Result.RoleId = Role; Result.HealthNormalized = Health;
		Result.ManaNormalized = Mana; Result.ReserveRounds = Ammo;
		return Result;
	}
	bool Init(FAuraSupplyRiskState& State, const TArray<FAuraRunSupplyMember>& Members)
	{
		FString Error; return State.Initialize(true, FGuid(52, 2026, 9, 4), 1, Members, Error);
	}
	const FAuraRunSupplyMember* Find(const FAuraSupplyRiskState& State, FName Id)
	{
		for (const FAuraRunSupplyMember& Member : State.GetMembers()) if (Member.MemberId == Id) return &Member;
		return nullptr;
	}
}

#define AURA_DAY52_TEST(ClassName, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.Gameplay.Day52." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AURA_DAY52_TEST(FAuraGameplayDay52SupplyAtomicAndCapped, "SupplyAtomicAndCapped")
bool FAuraGameplayDay52SupplyAtomicAndCapped::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error;
	AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura"), 1.0f) });
	TestEqual(TEXT("Full health rejects without consume"), State.UseMedkit(true, TEXT("a"), 1, false, Error), EAuraSupplyRiskResult::AlreadyFull);
	TestEqual(TEXT("Full rejection preserves two charges"), State.GetMembers()[0].Medkits, 2);
	FAuraRunSupplyMember Injured = AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura"), 0.7f);
	FAuraSupplyRiskState InjuredState; AuraGameplayDay52TestsPrivate::Init(InjuredState, { Injured });
	TestEqual(TEXT("Valid heal consumes once"), InjuredState.UseMedkit(true, TEXT("a"), 2, false, Error), EAuraSupplyRiskResult::Consumed);
	TestEqual(TEXT("Duplicate request is idempotent"), InjuredState.UseMedkit(true, TEXT("a"), 2, false, Error), EAuraSupplyRiskResult::DuplicateAccepted);
	TestEqual(TEXT("One charge remains"), InjuredState.GetMembers()[0].Medkits, 1);
	TestTrue(TEXT("Health is capped"), InjuredState.GetMembers()[0].HealthNormalized <= 1.0f);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52RoleResourceApplicability, "RoleResourceApplicability")
bool FAuraGameplayDay52RoleResourceApplicability::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error;
	AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura"), 1, .2f, 7), AuraGameplayDay52TestsPrivate::Member(TEXT("b"), TEXT("BungeeMan"), 1, .3f, 0) });
	State.UseResourcePack(true, TEXT("a"), 1, Error); State.UseResourcePack(true, TEXT("b"), 2, Error);
	const auto* Aura = AuraGameplayDay52TestsPrivate::Find(State, TEXT("a")); const auto* Bungee = AuraGameplayDay52TestsPrivate::Find(State, TEXT("b"));
	TestTrue(TEXT("Aura receives mana"), Aura && FMath::IsNearlyEqual(Aura->ManaNormalized, .7f));
	TestEqual(TEXT("Aura ammo is not applicable"), Aura ? Aura->ReserveRounds : -1, 7);
	TestEqual(TEXT("BungeeMan receives reserve rounds"), Bungee ? Bungee->ReserveRounds : -1, 24);
	TestTrue(TEXT("BungeeMan mana is not applicable"), Bungee && FMath::IsNearlyEqual(Bungee->ManaNormalized, .3f));
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52StationReceiptOnce, "StationReceiptOnce")
bool FAuraGameplayDay52StationReceiptOnce::RunTest(const FString& Parameters)
{
	FAuraRunSupplyMember Member = AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")); Member.Medkits = 1;
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { Member });
	TestEqual(TEXT("First personal station claim succeeds"), State.ClaimStation(true, TEXT("a"), 1, true, Error), EAuraSupplyRiskResult::Claimed);
	TestEqual(TEXT("Reconnect/client replay cannot claim twice"), State.ClaimStation(true, TEXT("a"), 2, false, Error), EAuraSupplyRiskResult::DuplicateAccepted);
	TestEqual(TEXT("Medkits remain capped"), State.GetMembers()[0].Medkits, 2);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52SupplyOwnerExtensionPreservesState, "SupplyOwnerExtensionPreservesState")
bool FAuraGameplayDay52SupplyOwnerExtensionPreservesState::RunTest(const FString& Parameters)
{
	FAuraRunSupplyMember Member = AuraGameplayDay52TestsPrivate::Member(TEXT("b"), TEXT("BungeeMan"), 1, 1, 0); Member.Medkits = 1; Member.EmergencyCooldownEndServerTime = 30;
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { Member });
	TestEqual(TEXT("Existing medkit count survives extension"), State.GetMembers()[0].Medkits, 1);
	TestEqual(TEXT("Changing pads cannot bypass existing cooldown"), State.UseEmergencyTerminal(true, TEXT("b"), TEXT("boss_pad"), 1, 20, true, Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Reconnect does not reset absolute cooldown"), State.GetMembers()[0].EmergencyCooldownEndServerTime, 30.0);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52EmergencyResourceNoSoftlock, "EmergencyResourceNoSoftlock")
bool FAuraGameplayDay52EmergencyResourceNoSoftlock::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("b"), TEXT("BungeeMan"), 1, 1, 0) });
	TestEqual(TEXT("Damage-cancelled channel consumes nothing"), State.UseEmergencyTerminal(true, TEXT("b"), TEXT("pad_a"), 1, 3, false, Error), EAuraSupplyRiskResult::Cancelled);
	TestEqual(TEXT("Completed channel restores six rounds"), State.UseEmergencyTerminal(true, TEXT("b"), TEXT("pad_a"), 2, 3, true, Error), EAuraSupplyRiskResult::Consumed);
	TestEqual(TEXT("Zero-resource player can continue"), State.GetMembers()[0].ReserveRounds, 6);
	TestEqual(TEXT("Cooldown is shared across pads"), State.UseEmergencyTerminal(true, TEXT("b"), TEXT("pad_b"), 3, 4, true, Error), EAuraSupplyRiskResult::Rejected);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52CacheOptionalAndBounded, "CacheOptionalAndBounded")
bool FAuraGameplayDay52CacheOptionalAndBounded::RunTest(const FString& Parameters)
{
	FString Error; FAuraSupplyRiskState Decline; AuraGameplayDay52TestsPrivate::Init(Decline, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")) });
	TestEqual(TEXT("Decision timeout defaults to decline"), Decline.ResolveCacheTimeoutOrChallenge(true, 10, 4, 8, 10, false, Error), EAuraSupplyRiskResult::Declined);
	TestTrue(TEXT("Decline leaves primary path open"), Decline.IsPrimaryPathOpen());
	FAuraSupplyRiskState Challenge; AuraGameplayDay52TestsPrivate::Init(Challenge, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")) });
	TestEqual(TEXT("Consent starts finite challenge"), Challenge.SubmitCacheConsent(true, TEXT("a"), true, 1, Error), EAuraSupplyRiskResult::ChallengeStarted);
	TestEqual(TEXT("Extra roster cannot exceed live cap"), Challenge.ResolveCacheTimeoutOrChallenge(true, 2, 4, 8, 10, true, Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Bounded challenge resolves"), Challenge.ResolveCacheTimeoutOrChallenge(true, 2, 2, 8, 10, true, Error), EAuraSupplyRiskResult::ChallengeCompleted);
	TestTrue(TEXT("Successful optional branch records one flag"), Challenge.IsCacheRewardEarned());
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52HazardRespectsRules, "HazardRespectsRules")
bool FAuraGameplayDay52HazardRespectsRules::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")) }); State.BeginHazardActivation(1);
	TestEqual(TEXT("Protected civilian is excluded"), State.TryHazardHit(true,true,true,true,true,true,true,true,TEXT("civilian"),false,false,true,false,1,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Safe-zone player is excluded"), State.TryHazardHit(true,true,true,true,true,true,true,true,TEXT("a"),true,true,false,false,1,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Friendly enemy is excluded"), State.TryHazardHit(true,true,true,true,true,true,true,true,TEXT("enemy"),false,false,false,true,1,Error), EAuraSupplyRiskResult::Rejected);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52HazardRegisteredSourceDamage, "HazardRegisteredSourceDamage")
bool FAuraGameplayDay52HazardRegisteredSourceDamage::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")) }); State.BeginHazardActivation(7);
	TestEqual(TEXT("Valid registered hazard damages eligible player"), State.TryHazardHit(true,true,true,true,true,true,true,true,TEXT("a"),true,false,false,false,7,Error), EAuraSupplyRiskResult::HitAccepted);
	TestEqual(TEXT("Per-activation membership prevents second hit"), State.TryHazardHit(true,true,true,true,true,true,true,true,TEXT("a"),true,false,false,false,7,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Hazard creates no player reward credit"), State.GetRewardCreditCount(), 0);
	return !HasAnyErrors();
}

AURA_DAY52_TEST(FAuraGameplayDay52HazardInvalidSourceFailsClosed, "HazardInvalidSourceFailsClosed")
bool FAuraGameplayDay52HazardInvalidSourceFailsClosed::RunTest(const FString& Parameters)
{
	FAuraSupplyRiskState State; FString Error; AuraGameplayDay52TestsPrivate::Init(State, { AuraGameplayDay52TestsPrivate::Member(TEXT("a"), TEXT("Aura")) }); State.BeginHazardActivation(1);
	TestEqual(TEXT("Missing identity fails closed"), State.TryHazardHit(true,false,true,true,true,true,true,true,TEXT("a"),true,false,false,false,1,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Missing ASC/avatar fails closed"), State.TryHazardHit(true,true,true,true,true,false,true,true,TEXT("a"),true,false,false,false,1,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("Stale or disabled activation fails closed"), State.TryHazardHit(true,true,true,true,true,true,false,false,TEXT("a"),true,false,false,false,1,Error), EAuraSupplyRiskResult::Rejected);
	TestEqual(TEXT("No invalid source damage occurs"), State.GetHazardDamageCount(), 0);
	return !HasAnyErrors();
}

#undef AURA_DAY52_TEST
#endif
