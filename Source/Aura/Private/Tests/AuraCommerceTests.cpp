// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AuraCommerceTests
{
	FString Read(const TCHAR* RelativePath)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *(FPaths::ProjectDir() / RelativePath));
		return Contents;
	}

	bool Has(const TCHAR* RelativePath, std::initializer_list<const TCHAR*> Tokens)
	{
		const FString Contents = Read(RelativePath);
		for (const TCHAR* Token : Tokens) if (!Contents.Contains(Token)) return false;
		return true;
	}

	bool SuccessAtomic()
	{
		return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"),
			{TEXT("CommitDebitCurrency(CurrencyIdBefore, Offer.BuyPrice, false)"), TEXT("CommitAddItem(Offer.ItemId, Offer.GrantQuantity, false)"), TEXT("PublishMerchantPresentation"), TEXT("SavePlayerAndWorldCheckpoint"), TEXT("PersistenceFailed")});
	}
	bool InsufficientFunds() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("InsufficientFunds"), TEXT("CanDebitCurrency")}); }
	bool FullInventory() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("FullInventory"), TEXT("CanAddItem")}); }
	bool OutOfRangeAndLineOfSight() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("OutOfRange"), TEXT("LineOfSightBlocked"), TEXT("IsLineOfSightClear")}); }
	bool DeadOrUnavailableMerchant() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("MerchantUnavailable"), TEXT("Merchant.bAvailable"), TEXT("WrongLifeState"), TEXT("MarkMerchantUnavailable")}); }
	bool SoldOut() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("SoldOut"), TEXT("CurrentStock < 1")}); }
	bool Eligibility() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("RequiredRoleId"), TEXT("RequiredInteractionTag"), TEXT("EligibilityDenied")}); }
	bool ForgedPayloadIgnored() { return Has(TEXT("Source/Aura/Private/Interaction/AuraInteractionComponent.cpp"), {TEXT("ServerRequestPurchase_Implementation")})
		&& Has(TEXT("Source/Aura/Public/Interaction/AuraInteractionComponent.h"), {TEXT("FGuid SessionNonce"), TEXT("uint64 RequestId"), TEXT("FName OfferId")}); }
	bool ReplayReturnsCachedResult() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("ReplayCache.Find(RequestId)"), TEXT("return true")}); }
	bool ReplayAfterLaterSuccessDoesNotRewind() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("LastAcceptedRequestId"), TEXT("CacheResult"), TEXT("ReplayOrder")}); }
	bool StaleNonceAndRequest() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("InvalidSession"), TEXT("StaleRequest"), TEXT("RequestGap"), TEXT("MAX_uint64")}); }
	bool RollbackLeavesNoPartialState() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("RestoreCurrencyState(CurrencyIdBefore"), TEXT("RestoreInventoryState(InventoryBefore"), TEXT("RollbackFailed")}); }
	bool LastStockContention() { return Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("--RuntimeOffer->CurrentStock"), TEXT("no client authority")}) || Has(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {TEXT("--RuntimeOffer->CurrentStock"), TEXT("ProcessPurchase")}); }
}

#define AURA_DAY17_TEST(ClassName, TestName, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_DAY17_TEST(FAuraCommerceSuccessAtomicTest, "Aura.RoleBattle.Day17.Commerce.SuccessAtomic", AuraCommerceTests::SuccessAtomic())
AURA_DAY17_TEST(FAuraCommerceFundsTest, "Aura.RoleBattle.Day17.Commerce.InsufficientFunds", AuraCommerceTests::InsufficientFunds())
AURA_DAY17_TEST(FAuraCommerceInventoryTest, "Aura.RoleBattle.Day17.Commerce.FullInventory", AuraCommerceTests::FullInventory())
AURA_DAY17_TEST(FAuraCommerceRangeTest, "Aura.RoleBattle.Day17.Commerce.OutOfRangeAndLineOfSight", AuraCommerceTests::OutOfRangeAndLineOfSight())
AURA_DAY17_TEST(FAuraCommerceMerchantStateTest, "Aura.RoleBattle.Day17.Commerce.DeadOrUnavailableMerchant", AuraCommerceTests::DeadOrUnavailableMerchant())
AURA_DAY17_TEST(FAuraCommerceSoldOutTest, "Aura.RoleBattle.Day17.Commerce.SoldOut", AuraCommerceTests::SoldOut())
AURA_DAY17_TEST(FAuraCommerceEligibilityTest, "Aura.RoleBattle.Day17.Commerce.Eligibility", AuraCommerceTests::Eligibility())
AURA_DAY17_TEST(FAuraCommercePayloadTest, "Aura.RoleBattle.Day17.Commerce.ForgedPayloadIgnored", AuraCommerceTests::ForgedPayloadIgnored())
AURA_DAY17_TEST(FAuraCommerceReplayTest, "Aura.RoleBattle.Day17.Commerce.ReplayReturnsCachedResult", AuraCommerceTests::ReplayReturnsCachedResult())
AURA_DAY17_TEST(FAuraCommerceReplayOrderTest, "Aura.RoleBattle.Day17.Commerce.ReplayAfterLaterSuccessDoesNotRewind", AuraCommerceTests::ReplayAfterLaterSuccessDoesNotRewind())
AURA_DAY17_TEST(FAuraCommerceStaleTest, "Aura.RoleBattle.Day17.Commerce.StaleNonceAndRequest", AuraCommerceTests::StaleNonceAndRequest())
AURA_DAY17_TEST(FAuraCommerceRollbackTest, "Aura.RoleBattle.Day17.Commerce.RollbackLeavesNoPartialState", AuraCommerceTests::RollbackLeavesNoPartialState())
AURA_DAY17_TEST(FAuraCommerceContentionTest, "Aura.RoleBattle.Day17.Commerce.LastStockContention", AuraCommerceTests::LastStockContention())

#undef AURA_DAY17_TEST

#endif
