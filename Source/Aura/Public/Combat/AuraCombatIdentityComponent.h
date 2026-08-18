// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/AuraCombatTypes.h"
#include "AuraCombatIdentityComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAuraCombatIdentityChanged, const FAuraCombatIdentity&);

/**
 * Owns one replicated combat identity for an avatar. The server initializes and
 * mutates the identity; clients receive it through normal component replication.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Combat Identity")
class AURA_API UAuraCombatIdentityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraCombatIdentityComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-only initialization/mutation entry point. Rejects invalid identity data. */
	bool InitializeIdentity(const FAuraCombatIdentity& InIdentity);
	/** Restores a pre-transaction identity, including the invalid initial state. */
	void RestoreIdentityForRollback(const FAuraCombatIdentity& InIdentity, bool bWasValid);

	/** Fired after an authoritative commit and after the matching client rep-notify. */
	FOnAuraCombatIdentityChanged OnIdentityChanged;

	const FAuraCombatIdentity& GetIdentity() const { return Identity; }

	UFUNCTION(BlueprintPure, Category = "Combat Identity")
	FAuraCombatIdentity GetIdentityCopy() const { return Identity; }

	UFUNCTION(BlueprintPure, Category = "Combat Identity")
	bool HasValidIdentity() const { return Identity.IsValid(); }

	/** Generic lookup used by AActor-based damage and targeting APIs. */
	static UAuraCombatIdentityComponent* FindForActor(AActor* Actor);
	static const UAuraCombatIdentityComponent* FindForActor(const AActor* Actor);

	/** Emits a warning no more than once for a given actor during the process lifetime. */
	static void LogMissingIdentityOnce(const AActor* Actor, const TCHAR* Context);

protected:
	UFUNCTION()
	void OnRep_Identity();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_Identity, Category = "Combat Identity")
	FAuraCombatIdentity Identity;

private:
	void LogIdentity(const TCHAR* NetworkSide) const;
};
