// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Gameplay/AuraMissionRewardSettlement.h"

namespace AuraGameplayDay53TestsPrivate
{
	FAuraSettlementMember Member(FName Id, bool bEligible=true)
	{
		FAuraSettlementMember M; M.MemberId=Id; M.RoleId=TEXT("Aura"); M.WalletGold=100; M.WalletCapacity=1000;
		M.bReadyAtStart=bEligible; M.ConnectedSeconds=bEligible?60:10; M.bAcceptedAction=bEligible;
		M.bPrimaryComplete=true; M.bBossExtractionComplete=true; M.bCacheComplete=true;
		M.EarnedMasteryBadgeId=TEXT("first_assault_success"); M.GameplayGuidanceMask=1; M.PendingGuidanceMask=4;
		M.LegacyTutorialMask=17; M.DurableInventoryFingerprint=TEXT("inventory-v1"); M.TransientSupplyCount=2;
		M.TransientNormalizedAmmo=24; M.TransientAugments={TEXT("quick_step")}; return M;
	}
	bool Init(FAuraMissionRewardSettlement& S, TArray<FAuraSettlementMember> Members)
	{ FString E; return S.Initialize(true,FGuid(53,1,1,1),FGuid(53,2,2,2),1,Members,E); }
	const FAuraSettlementMember* Find(const FAuraMissionRewardSettlement& S,FName Id)
	{ for(const auto& M:S.GetMembers()) if(M.MemberId==Id)return &M; return nullptr; }
}
#define AURA_DAY53_TEST(C,N) IMPLEMENT_SIMPLE_AUTOMATION_TEST(C,"Aura.Gameplay.Day53." N,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

AURA_DAY53_TEST(FAuraD53Fair,"SharedObjectiveFairReward") bool FAuraD53Fair::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; auto A=AuraGameplayDay53TestsPrivate::Member(TEXT("support")); auto B=AuraGameplayDay53TestsPrivate::Member(TEXT("damage")); AuraGameplayDay53TestsPrivate::Init(S,{A,B}); TestEqual(TEXT("Batch commits"),S.Commit(true,false,-1,false,false,E),EAuraSettlementResult::Committed); TestEqual(TEXT("Support receives same formula"),AuraGameplayDay53TestsPrivate::Find(S,TEXT("support"))->WalletGold,200); TestEqual(TEXT("Damage receives no last-hit bonus"),AuraGameplayDay53TestsPrivate::Find(S,TEXT("damage"))->WalletGold,200); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Idempotent,"SettlementIdempotent") bool FAuraD53Idempotent::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; AuraGameplayDay53TestsPrivate::Init(S,{AuraGameplayDay53TestsPrivate::Member(TEXT("a"))}); S.Commit(true,false,-1,false,false,E); TestEqual(TEXT("Duplicate terminal callback accepted without payout"),S.Commit(true,false,-1,false,false,E),EAuraSettlementResult::DuplicateAccepted); TestEqual(TEXT("One payout"),S.GetMembers()[0].WalletGold,200); TestEqual(TEXT("One receipt"),S.GetWorldReceiptCount(),1); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Atomic,"BatchCommitAllOrNothing") bool FAuraD53Atomic::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; AuraGameplayDay53TestsPrivate::Init(S,{AuraGameplayDay53TestsPrivate::Member(TEXT("a")),AuraGameplayDay53TestsPrivate::Member(TEXT("b"))}); TestEqual(TEXT("Injected second write fails"),S.Commit(true,false,1,false,false,E),EAuraSettlementResult::Rejected); TestTrue(TEXT("Neither wallet changes"),S.GetMembers()[0].WalletGold==100&&S.GetMembers()[1].WalletGold==100); TestFalse(TEXT("No manifest commit"),S.IsCommitted()); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Crash,"CrashAfterManifestNoReplay") bool FAuraD53Crash::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; AuraGameplayDay53TestsPrivate::Init(S,{AuraGameplayDay53TestsPrivate::Member(TEXT("a"))}); S.Commit(true,false,-1,false,false,E); const int32 Generation=S.GetManifestGeneration(); TestEqual(TEXT("Restart replay sees committed receipt"),S.Commit(true,false,-1,false,false,E),EAuraSettlementResult::DuplicateAccepted); TestEqual(TEXT("Manifest generation remains one"),S.GetManifestGeneration(),Generation); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Transient,"TransientStateExcluded") bool FAuraD53Transient::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; auto M=AuraGameplayDay53TestsPrivate::Member(TEXT("a")); const FString Inventory=M.DurableInventoryFingerprint; const int32 Ammo=M.TransientNormalizedAmmo; AuraGameplayDay53TestsPrivate::Init(S,{M}); S.Commit(true,false,-1,false,false,E); TestEqual(TEXT("Durable inventory remains frozen"),S.GetMembers()[0].DurableInventoryFingerprint,Inventory); TestEqual(TEXT("Run ammo is not normalized into durable state"),S.GetMembers()[0].TransientNormalizedAmmo,Ammo); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Cleanup,"CleanupIncludesIneligibleMembers") bool FAuraD53Cleanup::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; AuraGameplayDay53TestsPrivate::Init(S,{AuraGameplayDay53TestsPrivate::Member(TEXT("eligible")),AuraGameplayDay53TestsPrivate::Member(TEXT("zero"),false)}); S.Commit(true,false,-1,false,false,E); const auto* Z=AuraGameplayDay53TestsPrivate::Find(S,TEXT("zero")); TestTrue(TEXT("Ineligible marker clears"),Z&&Z->bMarkerCleared); TestEqual(TEXT("Ineligible payout is zero"),Z?Z->WalletGold:-1,100); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Expired,"ExpiredMemberGenerationProtected") bool FAuraD53Expired::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; AuraGameplayDay53TestsPrivate::Init(S,{AuraGameplayDay53TestsPrivate::Member(TEXT("expired")),AuraGameplayDay53TestsPrivate::Member(TEXT("survivor"))}); TestTrue(TEXT("Expiry restores newer hub generation"),S.ExpireMemberToNewHubGeneration(true,TEXT("expired"),2,777,E)); S.Commit(true,false,-1,false,false,E); const auto* X=AuraGameplayDay53TestsPrivate::Find(S,TEXT("expired")); const auto* V=AuraGameplayDay53TestsPrivate::Find(S,TEXT("survivor")); TestTrue(TEXT("Later hub mutation survives"),X&&X->ProfileGeneration==2&&X->WalletGold==777); TestEqual(TEXT("Survivor payout remains"),V?V->WalletGold:-1,200); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Capacity,"MaximumRewardCapacityAtEntry") bool FAuraD53Capacity::RunTest(const FString& Parameters)
{ TestTrue(TEXT("Capacity 100 admits"),FAuraMissionRewardSettlement::HasMaximumRewardCapacity(900,1000)); TestFalse(TEXT("Capacity 99 refuses"),FAuraMissionRewardSettlement::HasMaximumRewardCapacity(901,1000)); FAuraMissionRewardSettlement S; FString E; auto M=AuraGameplayDay53TestsPrivate::Member(TEXT("a")); M.WalletGold=950; M.WalletCapacity=1000; AuraGameplayDay53TestsPrivate::Init(S,{M}); TestEqual(TEXT("Defensive overflow rejects atomically"),S.Commit(true,false,-1,false,false,E),EAuraSettlementResult::Rejected); TestEqual(TEXT("Wallet is unchanged"),S.GetMembers()[0].WalletGold,950); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Guidance,"GameplayGuidanceDurableAllowlist") bool FAuraD53Guidance::RunTest(const FString& Parameters)
{ FAuraMissionRewardSettlement S; FString E; auto M=AuraGameplayDay53TestsPrivate::Member(TEXT("a")); AuraGameplayDay53TestsPrivate::Init(S,{M}); S.Commit(true,false,-1,false,false,E); TestEqual(TEXT("Accepted guidance merges"),S.GetMembers()[0].GameplayGuidanceMask,uint32(5)); TestEqual(TEXT("Legacy tutorial remains unchanged"),S.GetMembers()[0].LegacyTutorialMask,uint32(17)); TestEqual(TEXT("Inventory remains unchanged"),S.GetMembers()[0].DurableInventoryFingerprint,FString(TEXT("inventory-v1"))); return !HasAnyErrors(); }

AURA_DAY53_TEST(FAuraD53Migration,"SaveMigrationPreservesLegacy") bool FAuraD53Migration::RunTest(const FString& Parameters)
{ int32 Wallet=25; FString Inventory=TEXT("legacy-items"); uint32 Tutorial=7,Guidance=999; FString E; TestTrue(TEXT("Old save migrates"),FAuraMissionRewardSettlement::MigrateLegacyProfile(1,Wallet,Inventory,Tutorial,Guidance,E)); TestTrue(TEXT("Legacy values survive and guidance initializes"),Wallet==25&&Inventory==TEXT("legacy-items")&&Tutorial==7&&Guidance==0); TestFalse(TEXT("Unknown future version rejects"),FAuraMissionRewardSettlement::MigrateLegacyProfile(3,Wallet,Inventory,Tutorial,Guidance,E)); return !HasAnyErrors(); }

#undef AURA_DAY53_TEST
#endif
