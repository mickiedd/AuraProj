// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Combat/AuraCombatTypes.h"
#include "Interaction/CombatInterface.h"
#include "AuraCharacterBase.generated.h"

class UAuraCombatIdentityComponent;
class UAuraCombatStateComponent;
class UPassiveNiagaraComponent;
class UDebuffNiagaraComponent;
class UNiagaraSystem;
class UParticleSystem;
class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;
class USoundBase;
class UAuraDataAbility;
class UAuraAbilityDefinition;
class ULoadScreenSaveGame;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAppliedRoleStateChanged, const FAuraAppliedRoleState&);

UCLASS(Abstract)
class AURA_API AAuraCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	AAuraCharacterBase();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }
	const UAuraCombatIdentityComponent* GetCombatIdentityComponent() const { return CombatIdentityComponent; }
	const UAuraCombatStateComponent* GetCombatStateComponent() const { return CombatStateComponent; }
	UAuraCombatStateComponent* GetCombatStateComponentMutable() const { return CombatStateComponent; }
	EAuraCombatLifeState GetCombatLifeState() const;
	bool IsCombatAlive() const;
	const FAuraCombatIdentity& GetCombatIdentity() const;
	const FAuraCombatIdentity& GetDefaultCombatIdentity() const { return DefaultCombatIdentity; }
	FAuraCombatIdentity GetResolvedDefaultCombatIdentity() const { return BuildDefaultCombatIdentity(); }
	bool HasValidCombatIdentity() const;

	/** Complete authority-only spawn transaction. Never acts as a live role-switch API. */
	FAuraRoleApplicationResult ApplyRoleAtSpawn(FName InRole, const ULoadScreenSaveGame* SaveData = nullptr);

	/** Cosmetic/config-cache path. It never mutates identity, attributes, or ability specs. */
	FAuraRoleApplicationResult ApplyRolePresentation(FName AuthorizedRoleId);

	FName GetCharacterRole() const { return AppliedRoleState.RoleId; }
	const FAuraAppliedRoleState& GetAppliedRoleState() const { return AppliedRoleState; }
	FOnAppliedRoleStateChanged OnAppliedRoleStateChanged;

	/** Combat Interface */
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;	
	virtual void Die(const FVector& DeathImpulse) override;
	virtual FOnDeathSignature& GetOnDeathDelegate() override;
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
	virtual bool IsDead_Implementation() const override;
	virtual AActor* GetAvatar_Implementation() override;
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	virtual FTaggedMontage GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag) override;
	virtual int32 GetMinionCount_Implementation() override;
	virtual void IncremenetMinionCount_Implementation(int32 Amount) override;
	virtual ECharacterClass GetCharacterClass_Implementation() override;
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override;
	virtual USkeletalMeshComponent* GetWeapon_Implementation() override;
	virtual void SetIsBeingShocked_Implementation(bool bInShock) override;
	virtual bool IsBeingShocked_Implementation() const override;
	virtual bool IsInAir_Implementation() const override;
	virtual FOnDamageSignature& GetOnDamageSignature() override;
	/** end Combat Interface */

	FOnASCRegistered OnAscRegistered;
	FOnDeathSignature OnDeathDelegate;
	FOnDamageSignature OnDamageDelegate;

	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastHandleDeath(const FVector& DeathImpulse);

	/**
	 * Multicasts the gun's muzzle flash + fire sound to all clients. Called server-side by
	 * UAuraFireGun when it spawns the bullet. Impact + tracer FX are handled by the AAuraBullet
	 * projectile itself, so only muzzle cosmetics are relayed here. FX are Cascade
	 * UParticleSystem assets (SpawnEmitterAtLocation).
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayGunFireFX(const FVector_NetQuantize& MuzzleLocation,
		UParticleSystem* MuzzleFX, USoundBase* FireSound);

	UPROPERTY(EditAnywhere, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

	UPROPERTY(ReplicatedUsing=OnRep_Stunned, BlueprintReadOnly)
	bool bIsStunned = false;

	UPROPERTY(ReplicatedUsing=OnRep_Burned, BlueprintReadOnly)
	bool bIsBurned = false;

	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bIsBeingShocked = false;

	/** True while this character is mounted on the broom vehicle. Replicated to all clients. */
	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere)
	bool bIsMounted = false;

	UFUNCTION()
	virtual void OnRep_Stunned();

	UFUNCTION()
	virtual void OnRep_Burned();

	void SetCharacterClass(ECharacterClass InClass) { CharacterClass = InClass; }
protected:
	virtual void BeginPlay() override;
	virtual FAuraCombatIdentity BuildDefaultCombatIdentity() const;
	virtual FGameplayTag GetRequiredRoleEntityType() const;
	virtual FGameplayTag GetRequiredRoleControlType() const;

	/** Returns true only to the caller that won Alive -> Dying. */
	bool TryBeginCombatDeath();
	/** Completes normal avatar initialization by transitioning Respawning -> Alive. */
	bool MarkCombatReady();

	void HandleCombatLifeStateChanged(EAuraCombatLifeState NewState);
	void StartDay3NetworkProbe();
	void ExecuteDay3NetworkProbe();

	/** Client-originated Day 4 request used by the bounded network smoke. */
	UFUNCTION(Server, Reliable)
	void ServerRoleBattleDay4InvalidDamageProbe();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	TObjectPtr<UAuraCombatIdentityComponent> CombatIdentityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat State")
	TObjectPtr<UAuraCombatStateComponent> CombatStateComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Identity")
	FAuraCombatIdentity DefaultCombatIdentity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TailSocketName;

	UPROPERTY(BlueprintReadOnly)
	bool bDead = false;

	FTimerHandle Day3NetworkProbeTimerHandle;
	bool bDay3NetworkProbeStarted = false;
	bool bDay4NetworkProbeStarted = false;

	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float BaseWalkSpeed = 600.f;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	virtual void InitAbilityActorInfo();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;
	
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	virtual void InitializeDefaultAttributes() const;

	/** Loads secondary/vital/resistance attribute values from GameplayEffects.json and applies them via the C++ SetByCaller GE. */
	bool LoadAndApplySecondaryAttributes(bool bApplyZeroFallback = true) const;

	/**
	 * Applies this character's primary attributes from the role's numeric values (Strength/
	 * Intelligence/Resilience/Vigor) via the shared PrimaryAttributes_SetByCaller GE, then the
	 * shared Secondary/Vital GEs from the BP. Used for first-time player init when a Role is set.
	 */
	bool InitializeDefaultAttributesForRole(FName InRole, const FRoleDefaultInfo& RoleDefinition) const;

	void ClearRoleRuntimeState();
	bool LoadRoleRuntimeState(const FRoleDefaultInfo& RoleDefinition, FString& OutError);
	FAuraRoleApplicationResult ApplyRolePresentationFromDefinition(FName AuthorizedRoleId, const FRoleDefaultInfo& RoleDefinition);

	UFUNCTION()
	void OnRep_AppliedRoleState();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_AppliedRoleState, Category = "Applied Role")
	FAuraAppliedRoleState AppliedRoleState;

	/* Dissolve Effects */

	void Dissolve();

	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	UNiagaraSystem* BloodEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	USoundBase* DeathSound;

	/* Minions */
	
	int32 MinionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	ECharacterClass CharacterClass = ECharacterClass::Warrior;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> BurnDebuffComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> StunDebuffComponent;
	
private:

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupPassiveAbilities;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> StartupAbilityDefinitionObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> StartupPassiveAbilityDefinitionObjects;

	UPROPERTY(Transient)
	TObjectPtr<UObject> DefaultLMBAbilityDefinitionObject;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPassiveNiagaraComponent> HaloOfProtectionNiagaraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPassiveNiagaraComponent> LifeSiphonNiagaraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPassiveNiagaraComponent> ManaSiphonNiagaraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> EffectAttachComponent;
};
