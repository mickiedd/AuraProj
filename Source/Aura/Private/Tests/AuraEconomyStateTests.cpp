// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Economy/AuraEconomyConfig.h"

namespace AuraEconomyStateTests
{
	FString ReadProjectFile(const TCHAR* RelativePath)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *(FPaths::ProjectDir() / RelativePath));
		return Contents;
	}

	bool ContainsAll(const FString& Contents, std::initializer_list<const TCHAR*> Tokens)
	{
		for (const TCHAR* Token : Tokens)
		{
			if (!Contents.Contains(Token)) return false;
		}
		return true;
	}

	bool CheckedArithmeticContract()
	{
		int64 Value = 0;
		FString Error;
		return FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("9223372036854775807"), false, Value, Error)
			&& Value == MAX_int64
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("9223372036854775808"), false, Value, Error)
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("-1"), false, Value, Error);
	}

	bool OwnerOnlyContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraCurrencyComponent.cpp")),
			{ TEXT("DOREPLIFETIME_CONDITION"), TEXT("COND_OwnerOnly"), TEXT("Amount <= Maximum - Balance") })
			&& ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraInventoryComponent.cpp")),
			{ TEXT("FastArray"), TEXT("COND_OwnerOnly"), TEXT("MarkArrayDirty") });
	}

	bool StartingBalanceIdempotentContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp")),
			{ TEXT("NewEphemeralSession"), TEXT("EconomyInitializationCount"), TEXT("InitializeCurrency"), TEXT("if (!HasAuthority())") });
	}

	bool StackingAndCapacityContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraInventoryComponent.cpp")),
			{ TEXT("for (FAuraInventorySlot& Slot"), TEXT("while (Remaining > 0)"), TEXT("MaximumItemSlots"), TEXT("StackLimit") });
	}

	bool AllOrNothingContract()
	{
		const FString Inventory = ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraInventoryComponent.cpp"));
		return Inventory.Contains(TEXT("TArray<FAuraInventorySlot> Candidate = Inventory.Items"))
			&& Inventory.Contains(TEXT("if (!CanAddItem(ItemId, Quantity))"))
			&& Inventory.Contains(TEXT("Inventory.Items = MoveTemp(Candidate)"));
	}

	bool InvalidDefinitionContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraInventoryComponent.cpp")),
			{ TEXT("InvalidDefinition"), TEXT("FindDefinition"), TEXT("Slot.Quantity <= 0") });
	}

	bool ClientMutationRejectedContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraCurrencyComponent.cpp")),
			{ TEXT("NotAuthority"), TEXT("GetOwner()->HasAuthority") })
			&& ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Economy/AuraInventoryComponent.cpp")),
			{ TEXT("NotAuthority"), TEXT("GetOwner()->HasAuthority") })
			&& !ReadProjectFile(TEXT("Source/Aura/Public/Economy/AuraCurrencyComponent.h")).Contains(TEXT("UFUNCTION(Server"));
	}

	bool PawnRespawnPreservesEconomyContract()
	{
		return ContainsAll(ReadProjectFile(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp")),
			{ TEXT("CreateDefaultSubobject<UAuraCurrencyComponent>"), TEXT("CreateDefaultSubobject<UAuraInventoryComponent>") })
			&& ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCharacter.cpp")).Contains(TEXT("InitializeEconomyForNewProfileOnce"));
	}

	bool IndependentSessionContract()
	{
		const FString Header = ReadProjectFile(TEXT("Source/Aura/Public/Player/AuraPlayerState.h"));
		return Header.Contains(TEXT("EAuraEconomyInitializationState EconomyInitializationState"))
			&& Header.Contains(TEXT("ReplicatedUsing = OnRep_EconomyInitializationState"));
	}
}

#define AURA_DAY16_TEST(ClassName, TestName, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_DAY16_TEST(FAuraDay16CheckedArithmeticTest, "Aura.RoleBattle.Day16.Wallet.CheckedArithmetic", AuraEconomyStateTests::CheckedArithmeticContract())
AURA_DAY16_TEST(FAuraDay16StartingBalanceTest, "Aura.RoleBattle.Day16.Wallet.StartingBalanceIdempotent", AuraEconomyStateTests::StartingBalanceIdempotentContract())
AURA_DAY16_TEST(FAuraDay16StackingTest, "Aura.RoleBattle.Day16.Inventory.StackingAndSlotCapacity", AuraEconomyStateTests::StackingAndCapacityContract())
AURA_DAY16_TEST(FAuraDay16AllOrNothingTest, "Aura.RoleBattle.Day16.Inventory.AllOrNothingMutation", AuraEconomyStateTests::AllOrNothingContract())
AURA_DAY16_TEST(FAuraDay16InvalidDefinitionTest, "Aura.RoleBattle.Day16.Inventory.InvalidDefinition", AuraEconomyStateTests::InvalidDefinitionContract())
AURA_DAY16_TEST(FAuraDay16AuthorityTest, "Aura.RoleBattle.Day16.Authority.ClientMutationRejected", AuraEconomyStateTests::ClientMutationRejectedContract())
AURA_DAY16_TEST(FAuraDay16RespawnTest, "Aura.RoleBattle.Day16.Lifecycle.PawnRespawnPreservesEconomy", AuraEconomyStateTests::PawnRespawnPreservesEconomyContract())
AURA_DAY16_TEST(FAuraDay16IndependentSessionTest, "Aura.RoleBattle.Day16.Lifecycle.IndependentSessionInitialization", AuraEconomyStateTests::IndependentSessionContract())

#undef AURA_DAY16_TEST

#endif
