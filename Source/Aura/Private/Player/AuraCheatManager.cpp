// Copyright Druid Mechanics

#include "Player/AuraCheatManager.h"
#include "Player/AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Client/AuraClientDisconnectHandler.h"
#include "Game/ServerTravelComponent.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"

AAuraPlayerController* UAuraCheatManager::GetAuraPC() const
{
	return Cast<AAuraPlayerController>(GetOuter());
}

UAuraAbilitySystemComponent* UAuraCheatManager::GetASC() const
{
	if (const AAuraPlayerController* PC = GetAuraPC())
	{
		// Player ASC lives on PlayerState in Aura; fall back to the pawn for
		// cases where the ASC is authored on the character instead.
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PC->PlayerState))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				return Cast<UAuraAbilitySystemComponent>(ASC);
			}
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			return Cast<UAuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn));
		}
	}
	return nullptr;
}

const UAuraAttributeSet* UAuraCheatManager::GetAuraAS() const
{
	if (UAuraAbilitySystemComponent* ASC = GetASC())
	{
		return ASC->GetSet<UAuraAttributeSet>();
	}
	return nullptr;
}

void UAuraCheatManager::GiveAllAbilities()
{
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->FullAbilities();
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::GiveAllAbilities: no owning PlayerController."));
	}
}

void UAuraCheatManager::KickOutSelf()
{
	AAuraPlayerController* PC = GetAuraPC();
	if (PC == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::KickOutSelf: no owning PlayerController."));
		return;
	}

	// Reuse the same server-lost path the real network/heartbeat detectors use:
	// stash the Login-screen message and ClientTravel back to the Login level. The
	// travel drops the dedicated-server connection, achieving the "disconnect + go
	// back to login" effect. Idempotent via UAuraClientDisconnectHandler::bHandled.
	if (UAuraClientDisconnectHandler* DisconnectHandler = PC->FindComponentByClass<UAuraClientDisconnectHandler>())
	{
		UE_LOG(LogAura, Log, TEXT("KickOutSelf: requesting server-lost -> Login travel."));
		DisconnectHandler->RequestServerLost(TEXT("Kicked out by cheat (KickOutSelf)."));
		PC->ClientMessage(TEXT("KickOutSelf: disconnecting and returning to Login."));
		return;
	}

	// Fallback: no disconnect handler wired up — travel directly to Login.
	UE_LOG(LogAura, Warning, TEXT("KickOutSelf: no AuraClientDisconnectHandler on the controller; falling back to direct ClientTravel to Login."));
	PC->ClientTravel(UServerTravelComponent::LoginLevelPath, TRAVEL_Absolute);
	PC->ClientMessage(TEXT("KickOutSelf: traveling to Login (no disconnect handler found)."));
}

void UAuraCheatManager::RefillVitals()
{
	RefillHealth();
	RefillMana();
}

void UAuraCheatManager::RefillHealth()
{
	UAuraAbilitySystemComponent* ASC = GetASC();
	const UAuraAttributeSet* AS = GetAuraAS();
	if (!ASC || !AS)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::RefillHealth: missing ASC/AttributeSet."));
		return;
	}

	const float MaxHealth = AS->GetMaxHealth();
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), MaxHealth);
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->ClientMessage(FString::Printf(TEXT("Health refilled to %.0f / %.0f."), MaxHealth, MaxHealth));
	}
}

void UAuraCheatManager::RefillMana()
{
	UAuraAbilitySystemComponent* ASC = GetASC();
	const UAuraAttributeSet* AS = GetAuraAS();
	if (!ASC || !AS)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::RefillMana: missing ASC/AttributeSet."));
		return;
	}

	const float MaxMana = AS->GetMaxMana();
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetManaAttribute(), MaxMana);
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->ClientMessage(FString::Printf(TEXT("Mana refilled to %.0f / %.0f."), MaxMana, MaxMana));
	}
}

void UAuraCheatManager::SetHealth(float NewHealth)
{
	UAuraAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::SetHealth: missing ASC."));
		return;
	}

	// PreAttributeChange clamps Health to [0, MaxHealth].
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), NewHealth);
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->ClientMessage(FString::Printf(TEXT("Health set to %.0f."), NewHealth));
	}
}

void UAuraCheatManager::SetMana(float NewMana)
{
	UAuraAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::SetMana: missing ASC."));
		return;
	}

	// PreAttributeChange clamps Mana to [0, MaxMana].
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetManaAttribute(), NewMana);
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->ClientMessage(FString::Printf(TEXT("Mana set to %.0f."), NewMana));
	}
}

void UAuraCheatManager::AddXP(int32 InXP)
{
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		if (AAuraPlayerState* AuraPS = Cast<AAuraPlayerState>(PC->PlayerState))
		{
			AuraPS->AddToXP(InXP);
			PC->ClientMessage(FString::Printf(TEXT("Added %d XP."), InXP));
			return;
		}
	}
	UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::AddXP: no AuraPlayerState."));
}

void UAuraCheatManager::AddLevel(int32 InLevel)
{
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		if (AAuraPlayerState* AuraPS = Cast<AAuraPlayerState>(PC->PlayerState))
		{
			AuraPS->AddToLevel(InLevel);
			PC->ClientMessage(FString::Printf(TEXT("Added %d level(s)."), InLevel));
			return;
		}
	}
	UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::AddLevel: no AuraPlayerState."));
}

void UAuraCheatManager::AddAttributePoints(int32 InPoints)
{
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		if (AAuraPlayerState* AuraPS = Cast<AAuraPlayerState>(PC->PlayerState))
		{
			AuraPS->AddToAttributePoints(InPoints);
			PC->ClientMessage(FString::Printf(TEXT("Added %d attribute point(s)."), InPoints));
			return;
		}
	}
	UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::AddAttributePoints: no AuraPlayerState."));
}

void UAuraCheatManager::AddSpellPoints(int32 InPoints)
{
	if (AAuraPlayerController* PC = GetAuraPC())
	{
		if (AAuraPlayerState* AuraPS = Cast<AAuraPlayerState>(PC->PlayerState))
		{
			AuraPS->AddToSpellPoints(InPoints);
			PC->ClientMessage(FString::Printf(TEXT("Added %d spell point(s)."), InPoints));
			return;
		}
	}
	UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::AddSpellPoints: no AuraPlayerState."));
}

void UAuraCheatManager::DumpAttributes()
{
	const UAuraAttributeSet* AS = GetAuraAS();
	if (!AS)
	{
		UE_LOG(LogAura, Warning, TEXT("AuraCheatManager::DumpAttributes: missing AttributeSet."));
		return;
	}

	UE_LOG(LogAura, Log, TEXT("---- Aura Attributes for %s ----"), *GetNameSafe(GetAuraPC()));
	UE_LOG(LogAura, Log, TEXT("Health=%.1f / MaxHealth=%.1f"), AS->GetHealth(), AS->GetMaxHealth());
	UE_LOG(LogAura, Log, TEXT("Mana=%.1f / MaxMana=%.1f"), AS->GetMana(), AS->GetMaxMana());
	UE_LOG(LogAura, Log, TEXT("Strength=%.1f Intelligence=%.1f Resilience=%.1f Vigor=%.1f"),
		AS->GetStrength(), AS->GetIntelligence(), AS->GetResilience(), AS->GetVigor());
	UE_LOG(LogAura, Log, TEXT("Armor=%.1f ArmorPen=%.1f BlockChance=%.1f"),
		AS->GetArmor(), AS->GetArmorPenetration(), AS->GetBlockChance());
	UE_LOG(LogAura, Log, TEXT("CritChance=%.1f CritDamage=%.1f CritResist=%.1f"),
		AS->GetCriticalHitChance(), AS->GetCriticalHitDamage(), AS->GetCriticalHitResistance());
	UE_LOG(LogAura, Log, TEXT("HealthRegen=%.1f ManaRegen=%.1f"),
		AS->GetHealthRegeneration(), AS->GetManaRegeneration());
	UE_LOG(LogAura, Log, TEXT("Resistances: Fire=%.1f Lightning=%.1f Arcane=%.1f Physical=%.1f"),
		AS->GetFireResistance(), AS->GetLightningResistance(), AS->GetArcaneResistance(), AS->GetPhysicalResistance());

	if (AAuraPlayerController* PC = GetAuraPC())
	{
		PC->ClientMessage(TEXT("Attributes dumped to log (LogAura)."));
	}
}