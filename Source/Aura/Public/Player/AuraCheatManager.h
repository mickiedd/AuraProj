// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "AuraCheatManager.generated.h"

class AAuraPlayerController;
class UAuraAbilitySystemComponent;
class UAuraAttributeSet;

/**
 * Project-specific cheat commands for Aura.
 *
 * Wired to AAuraPlayerController through its CheatClass. Because these are
 * UFUNCTION(Exec) on a UCheatManager, the console only dispatches them while
 * cheats are enabled (standalone / listen-server host, or after `enablecheats`
 * on a dedicated server) — they cannot be invoked by ordinary remote clients,
 * so it is safe to keep them compiled into shipping builds.
 */
UCLASS()
class AURA_API UAuraCheatManager : public UCheatManager
{
	GENERATED_BODY()
public:
	// ---- Abilities ----------------------------------------------------------

	/** Grant and equip every ability on the controlled character. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void GiveAllAbilities();

	/** Disconnect from the dedicated server and travel back to the Login level. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void KickOutSelf();

	// ---- Vitals -------------------------------------------------------------

	/** Set Health and Mana back to their current maximums. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void RefillVitals();

	/** Set Health back to MaxHealth. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void RefillHealth();

	/** Set Mana back to MaxMana. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void RefillMana();

	/** Set Health directly (clamped to MaxHealth by the AttributeSet). */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void SetHealth(float NewHealth);

	/** Set Mana directly (clamped to MaxMana by the AttributeSet). */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void SetMana(float NewMana);

	// ---- Progression --------------------------------------------------------

	/** Grant XP to the player. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void AddXP(int32 InXP);

	/** Add to the player's level. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void AddLevel(int32 InLevel);

	/** Grant spendable attribute points. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void AddAttributePoints(int32 InPoints);

	/** Grant spendable spell points. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void AddSpellPoints(int32 InPoints);

	// ---- Debug --------------------------------------------------------------

	/** Dump all Aura attributes for the controlled character to the log. */
	UFUNCTION(Exec, Category="Aura|Cheats")
	void DumpAttributes();

protected:
	/** The owning player controller (the outer of this cheat manager). */
	AAuraPlayerController* GetAuraPC() const;

	/** GAS component for the controlled character, or nullptr. */
	UAuraAbilitySystemComponent* GetASC() const;

	/** Aura attribute set for the controlled character, or nullptr. */
	const UAuraAttributeSet* GetAuraAS() const;
};