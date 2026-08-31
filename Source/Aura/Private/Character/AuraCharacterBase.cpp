// Copyright Druid Mechanics


#include "Character/AuraCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "AbilitySystem/Passive/PassiveNiagaraComponent.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "AuraAttributeGameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatRules.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraDeathPolicyDispatcher.h"
#include "Data/AuraGameplayConfig.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/LoadScreenSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Parse.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "Player/AuraPlayerState.h"
#include "Game/AuraGameModeBase.h"

#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "AuraAbilityTypes.h"

AAuraCharacterBase::AAuraCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	CombatIdentityComponent = CreateDefaultSubobject<UAuraCombatIdentityComponent>(TEXT("CombatIdentityComponent"));
	CombatStateComponent = CreateDefaultSubobject<UAuraCombatStateComponent>(TEXT("CombatStateComponent"));
	CombatStateComponent->OnLifeStateChanged.AddUObject(this, &AAuraCharacterBase::HandleCombatLifeStateChanged);
	
	BurnDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("BurnDebuffComponent");
	BurnDebuffComponent->SetupAttachment(GetRootComponent());
	BurnDebuffComponent->DebuffTag = GameplayTags.Debuff_Burn;

	StunDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("StunDebuffComponent");
	StunDebuffComponent->SetupAttachment(GetRootComponent());
	StunDebuffComponent->DebuffTag = GameplayTags.Debuff_Stun;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EffectAttachComponent = CreateDefaultSubobject<USceneComponent>("EffectAttachPoint");
	EffectAttachComponent->SetupAttachment(GetRootComponent());
	HaloOfProtectionNiagaraComponent = CreateDefaultSubobject<UPassiveNiagaraComponent>("HaloOfProtectionComponent");
	HaloOfProtectionNiagaraComponent->SetupAttachment(EffectAttachComponent);
	LifeSiphonNiagaraComponent = CreateDefaultSubobject<UPassiveNiagaraComponent>("LifeSiphonNiagaraComponent");
	LifeSiphonNiagaraComponent->SetupAttachment(EffectAttachComponent);
	ManaSiphonNiagaraComponent = CreateDefaultSubobject<UPassiveNiagaraComponent>("ManaSiphonNiagaraComponent");
	ManaSiphonNiagaraComponent->SetupAttachment(EffectAttachComponent);

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AAuraCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	EffectAttachComponent->SetWorldRotation(FRotator::ZeroRotator);
}

void AAuraCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraCharacterBase, bIsStunned);
	DOREPLIFETIME(AAuraCharacterBase, bIsBurned);
	DOREPLIFETIME(AAuraCharacterBase, bIsBeingShocked);
	DOREPLIFETIME(AAuraCharacterBase, bIsMounted);
	DOREPLIFETIME(AAuraCharacterBase, AppliedRoleState);
}

float AAuraCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float DamageTaken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	OnDamageDelegate.Broadcast(DamageTaken);
	return DamageTaken;
}

UAbilitySystemComponent* AAuraCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const FAuraCombatIdentity& AAuraCharacterBase::GetCombatIdentity() const
{
	check(CombatIdentityComponent);
	return CombatIdentityComponent->GetIdentity();
}

bool AAuraCharacterBase::HasValidCombatIdentity() const
{
	return CombatIdentityComponent && CombatIdentityComponent->HasValidIdentity();
}

FAuraRoleApplicationResult AAuraCharacterBase::ApplyRoleAtSpawn(FName InRole, const ULoadScreenSaveGame* SaveData)
{
	if (!HasAuthority())
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::NotAuthority,
			TEXT("Only the server may apply a role at spawn."));
	}

	URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	if (!RoleInfo)
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::RoleServiceUnavailable,
			TEXT("The authoritative role registry is unavailable."));
	}
	const FRoleDefaultInfo* Candidate = RoleInfo->RoleInformation.Find(InRole);
	if (!Candidate)
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::UnknownRole,
			FString::Printf(TEXT("Role '%s' is not in the published registry."), *InRole.ToString()));
	}

	const FGameplayTag RequiredEntity = GetRequiredRoleEntityType();
	const FGameplayTag RequiredControl = GetRequiredRoleControlType();
	if (!RequiredEntity.IsValid() || !RequiredControl.IsValid()
		|| !Candidate->EntityType.MatchesTagExact(RequiredEntity)
		|| !Candidate->ControlType.MatchesTagExact(RequiredControl))
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::IncompatibleActorShell,
			FString::Printf(TEXT("Role '%s' (%s/%s) is incompatible with actor shell %s (%s/%s)."),
				*InRole.ToString(), *Candidate->EntityType.ToString(), *Candidate->ControlType.ToString(), *GetNameSafe(this),
				*RequiredEntity.ToString(), *RequiredControl.ToString()));
	}

	UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	if (!AuraASC || !AuraASC->GetOwnerActor() || AuraASC->GetAvatarActor() != this || !CombatIdentityComponent
		|| (AuraPlayerState && !AuraPlayerState->HasAuthority()))
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::AbilityActorInfoMissing,
			TEXT("InitAbilityActorInfo/AbilityActorInfoSet must complete for this avatar before role application."));
	}
	if (AppliedRoleState.IsValid() && AppliedRoleState.RoleId != InRole)
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::UnsupportedLiveSwitch,
			FString::Printf(TEXT("Live role switching from '%s' to '%s' is disabled."),
				*AppliedRoleState.RoleId.ToString(), *InRole.ToString()));
	}

	FAuraAppliedRoleState CandidateState;
	CandidateState.RoleId = InRole;
	CandidateState.EntityTypeTag = Candidate->EntityType;
	CandidateState.EconomyProfileTag = Candidate->EconomyProfile;
	CandidateState.InteractionProfileTag = Candidate->InteractionProfile;
	FAuraCombatIdentity CandidateIdentity;
	CandidateIdentity.FactionTag = Candidate->Faction;
	CandidateIdentity.ControlTypeTag = Candidate->ControlType;
	CandidateIdentity.CombatProfileTag = Candidate->CombatProfile;
	CandidateIdentity.DeathPolicyTag = Candidate->DeathPolicy;
	CandidateIdentity.bTargetable = Candidate->bTargetable;
	CandidateIdentity.bCanAttack = Candidate->bCanAttack;
	CandidateIdentity.bCanBeDamaged = Candidate->bCanBeDamaged;
	CandidateIdentity.bAllowFriendlyFire = Candidate->bAllowFriendlyFire;
	if (!CandidateState.IsValid() || !CandidateIdentity.IsValid()
		|| !Candidate->SkeletalMesh || !Candidate->AnimBlueprintClass
		|| (Candidate->WeaponMesh && Candidate->WeaponSocketName.IsNone()))
	{
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::InvalidRoleCandidate,
			TEXT("The staged role is missing required identity, presentation, or equipment data."));
	}

	FString GrantError;
	if (!AuraASC->ValidateRoleGrantSet(InRole, *Candidate, SaveData, GrantError))
	{
		const EAuraRoleApplicationError Error = AuraASC->GetRoleGrantLedger().bInitialized
			&& AuraASC->GetRoleGrantLedger().GrantedRoleId != InRole
			? EAuraRoleApplicationError::UnsupportedLiveSwitch
			: EAuraRoleApplicationError::GrantReconciliationFailed;
		return FAuraRoleApplicationResult::Failure(InRole, Error, MoveTemp(GrantError));
	}

	const FAuraAppliedRoleState PreviousState = AppliedRoleState;
	const FAuraCombatIdentity PreviousIdentity = CombatIdentityComponent->GetIdentity();
	const bool bPreviousIdentityValid = CombatIdentityComponent->HasValidIdentity();
	const FName PreviousPlayerRole = AuraPlayerState ? AuraPlayerState->GetRole() : NAME_None;
	FAuraRoleGrantTransactionSnapshot GrantSnapshot;
	AuraASC->CaptureRoleGrantState(GrantSnapshot);
	auto RollbackTransaction = [&]()
	{
		AuraASC->RollbackRoleGrantState(GrantSnapshot);
		CombatIdentityComponent->RestoreIdentityForRollback(PreviousIdentity, bPreviousIdentityValid);
		if (AuraPlayerState && AuraPlayerState->GetRole() != PreviousPlayerRole)
		{
			AuraPlayerState->SetRole(PreviousPlayerRole);
		}
		AppliedRoleState = PreviousState;
		if (PreviousState.IsValid()) ApplyRolePresentation(PreviousState.RoleId); else ClearRoleRuntimeState();
	};
	const FAuraRoleApplicationResult Presentation = ApplyRolePresentationFromDefinition(InRole, *Candidate);
	if (!Presentation.bSuccess)
	{
		return Presentation;
	}

	if (!AuraASC->ApplyRoleGrantSet(InRole, RoleInfo->RoleDefinitionVersion, *Candidate, SaveData, GrantError))
	{
		RollbackTransaction();
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::GrantReconciliationFailed,
			MoveTemp(GrantError));
	}

	const bool bAttributesAlreadyInitialized = AuraASC->GetRoleGrantLedger().bAttributesInitialized;
	if (!bAttributesAlreadyInitialized)
	{
		if (SaveData && !SaveData->bFirstTimeLoadIn)
		{
			if (!UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(this, AbilitySystemComponent,
				const_cast<ULoadScreenSaveGame*>(SaveData)))
			{
				RollbackTransaction();
				return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::InvalidRoleCandidate,
					TEXT("Saved attribute initialization could not stage valid GameplayEffect specs."));
			}
		}
		else if (!InitializeDefaultAttributesForRole(InRole, *Candidate))
		{
			RollbackTransaction();
			return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::InvalidRoleCandidate,
				TEXT("Role attribute initialization could not stage valid GameplayEffect specs."));
		}
		AuraASC->MarkRoleAttributesInitialized();
	}
	else
	{
		UAuraAbilitySystemLibrary::TopOffVitalAttributes(AbilitySystemComponent, this);
	}

	AppliedRoleState = CandidateState;
	if (!CombatIdentityComponent || !CombatIdentityComponent->InitializeIdentity(CandidateIdentity))
	{
		RollbackTransaction();
		return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::InvalidRoleCandidate,
			TEXT("The authoritative combat identity could not be committed."));
	}
	if (AuraPlayerState)
	{
		if (!AuraPlayerState->SetRole(InRole))
		{
			RollbackTransaction();
			return FAuraRoleApplicationResult::Failure(InRole, EAuraRoleApplicationError::NotAuthority,
				TEXT("PlayerState rejected the authoritative role commit."));
		}
	}
	OnAppliedRoleStateChanged.Broadcast(AppliedRoleState);
	ForceNetUpdate();
	UE_LOG(LogAura, Display,
		TEXT("[Day6Role][Server] Actor=%s Role=%s Entity=%s Combat=%s Economy=%s Interaction=%s RoleSpecs=%d ReusedLedger=%d"),
		*GetNameSafe(this), *InRole.ToString(), *CandidateState.EntityTypeTag.ToString(),
		*CandidateIdentity.CombatProfileTag.ToString(), *CandidateState.EconomyProfileTag.ToString(),
		*CandidateState.InteractionProfileTag.ToString(), AuraASC->GetRoleGrantLedger().AbilitySpecHandles.Num(),
		AuraASC->GetRoleGrantLedger().bInitialized && bAttributesAlreadyInitialized);
	return FAuraRoleApplicationResult::Success(InRole);
}

FAuraRoleApplicationResult AAuraCharacterBase::ApplyRolePresentation(FName AuthorizedRoleId)
{
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	const FRoleDefaultInfo* RoleDefinition = RoleInfo ? RoleInfo->RoleInformation.Find(AuthorizedRoleId) : nullptr;
	if (!RoleDefinition)
	{
		UE_LOG(LogAura, Error, TEXT("[RolePresentation] Missing local definition for authorized role '%s' on %s."),
			*AuthorizedRoleId.ToString(), *GetNameSafe(this));
		return FAuraRoleApplicationResult::Failure(AuthorizedRoleId, EAuraRoleApplicationError::PresentationFailed,
			TEXT("The local presentation definition is missing; no fallback identity was constructed."));
	}
	return ApplyRolePresentationFromDefinition(AuthorizedRoleId, *RoleDefinition);
}

FAuraRoleApplicationResult AAuraCharacterBase::ApplyRolePresentationFromDefinition(
	FName AuthorizedRoleId, const FRoleDefaultInfo& RoleDefinition)
{
	FString Error;
	if (!LoadRoleRuntimeState(RoleDefinition, Error))
	{
		return FAuraRoleApplicationResult::Failure(AuthorizedRoleId, EAuraRoleApplicationError::PresentationFailed, MoveTemp(Error));
	}
	UE_LOG(LogAura, Display, TEXT("[RolePresentation][%s] Actor=%s Role=%s Weapon=%s Tip=%s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"), *GetNameSafe(this), *AuthorizedRoleId.ToString(),
		*GetNameSafe(RoleDefinition.WeaponMesh.Get()), *RoleDefinition.WeaponTipSocketName.ToString());
	return FAuraRoleApplicationResult::Success(AuthorizedRoleId);
}

void AAuraCharacterBase::ClearRoleRuntimeState()
{
	// Detach while the old body/socket is still valid. Clearing the parent mesh
	// first makes child transform updates query a socket on an empty mesh.
	Weapon->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	GetMesh()->SetAnimInstanceClass(nullptr);
	GetMesh()->SetSkeletalMeshAsset(nullptr);
	Weapon->SetSkeletalMeshAsset(nullptr);
	WeaponTipSocketName = NAME_None;
	LeftHandSocketName = NAME_None;
	RightHandSocketName = NAME_None;
	TailSocketName = NAME_None;
	DissolveMaterialInstance = nullptr;
	WeaponDissolveMaterialInstance = nullptr;
	BloodEffect = nullptr;
	DeathSound = nullptr;
	StartupAbilities.Reset();
	StartupPassiveAbilities.Reset();
	StartupAbilityDefinitionObjects.Reset();
	StartupPassiveAbilityDefinitionObjects.Reset();
	DefaultLMBAbilityDefinitionObject = nullptr;
}

bool AAuraCharacterBase::LoadRoleRuntimeState(const FRoleDefaultInfo& RoleDefinition, FString& OutError)
{
	if (!RoleDefinition.SkeletalMesh || !RoleDefinition.AnimBlueprintClass)
	{
		OutError = TEXT("Role presentation requires a body mesh and animation blueprint.");
		return false;
	}
	for (const TObjectPtr<UObject>& Object : RoleDefinition.StartupAbilityDefinitions)
	{
		if (!Cast<UAuraAbilityDefinition>(Object.Get()))
		{
			OutError = TEXT("Role contains an unresolved startup ability definition.");
			return false;
		}
	}
	for (const TObjectPtr<UObject>& Object : RoleDefinition.StartupPassiveAbilityDefinitions)
	{
		if (!Cast<UAuraAbilityDefinition>(Object.Get()))
		{
			OutError = TEXT("Role contains an unresolved passive ability definition.");
			return false;
		}
	}

	ClearRoleRuntimeState();
	GetMesh()->SetSkeletalMeshAsset(RoleDefinition.SkeletalMesh);
	GetMesh()->SetAnimInstanceClass(RoleDefinition.AnimBlueprintClass);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (RoleDefinition.WeaponMesh)
	{
		Weapon->SetSkeletalMeshAsset(RoleDefinition.WeaponMesh);
		Weapon->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RoleDefinition.WeaponSocketName);
	}
	WeaponTipSocketName = RoleDefinition.WeaponTipSocketName;
	LeftHandSocketName = RoleDefinition.LeftHandSocketName;
	RightHandSocketName = RoleDefinition.RightHandSocketName;
	TailSocketName = RoleDefinition.TailSocketName;
	DissolveMaterialInstance = RoleDefinition.DissolveMaterialInstance;
	WeaponDissolveMaterialInstance = RoleDefinition.WeaponDissolveMaterialInstance;
	BloodEffect = RoleDefinition.BloodEffect;
	DeathSound = RoleDefinition.DeathSound;
	StartupAbilities = RoleDefinition.StartupAbilities;
	StartupPassiveAbilities = RoleDefinition.StartupPassiveAbilities;
	StartupAbilityDefinitionObjects = RoleDefinition.StartupAbilityDefinitions;
	StartupPassiveAbilityDefinitionObjects = RoleDefinition.StartupPassiveAbilityDefinitions;
	DefaultLMBAbilityDefinitionObject = RoleDefinition.DefaultLMBAbilityDefinition;
	if (RoleDefinition.DefaultLMBAbility)
	{
		StartupAbilities.AddUnique(RoleDefinition.DefaultLMBAbility);
	}
	return true;
}

void AAuraCharacterBase::OnRep_AppliedRoleState()
{
	if (!AppliedRoleState.IsValid()) return;
	const FAuraRoleApplicationResult Result = ApplyRolePresentation(AppliedRoleState.RoleId);
	if (!Result.bSuccess)
	{
		UE_LOG(LogAura, Error, TEXT("[Day6Role][Client] PresentationError Role=%s Message=%s"),
			*AppliedRoleState.RoleId.ToString(), *Result.Message);
		return;
	}
	OnAppliedRoleStateChanged.Broadcast(AppliedRoleState);
	UE_LOG(LogAura, Display, TEXT("[Day6Role][Client] Actor=%s Role=%s Entity=%s Economy=%s Interaction=%s Presentation=1"),
		*GetNameSafe(this), *AppliedRoleState.RoleId.ToString(), *AppliedRoleState.EntityTypeTag.ToString(),
		*AppliedRoleState.EconomyProfileTag.ToString(), *AppliedRoleState.InteractionProfileTag.ToString());
}

UAnimMontage* AAuraCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void AAuraCharacterBase::Die(const FVector& DeathImpulse)
{
	if (!HasAuthority() || !CombatStateComponent)
	{
		return;
	}

	if (CombatStateComponent->GetLifeState() == EAuraCombatLifeState::Alive
		&& !TryBeginCombatDeath())
	{
		return;
	}

	if (CombatStateComponent->GetLifeState() != EAuraCombatLifeState::Dying)
	{
		return;
	}

	Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
	if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
	{
		if (UAuraDeathPolicyDispatcher* Dispatcher = GameMode->GetDeathPolicyDispatcherMutable())
		{
			Dispatcher->DispatchDeath(CombatStateComponent->GetLastDeathEvent());
		}
	}
	MulticastHandleDeath(DeathImpulse);
	CombatStateComponent->TryEnterDead();
}

FGameplayTag AAuraCharacterBase::GetAuraTargetKind() const
{
	return FAuraGameplayTags::Get().Target_Kind_World;
}

FText AAuraCharacterBase::GetAuraTargetDisplayName() const
{
	return FText::FromString(GetName());
}

float AAuraCharacterBase::GetAuraTargetHealth() const
{
	const UAuraAttributeSet* Attributes = Cast<UAuraAttributeSet>(AttributeSet);
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float AAuraCharacterBase::GetAuraTargetMaxHealth() const
{
	const UAuraAttributeSet* Attributes = Cast<UAuraAttributeSet>(AttributeSet);
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

void AAuraCharacterBase::SetPendingFatalDamageContext(const FAuraFatalDamageContext& InContext)
{
	if (HasAuthority())
	{
		PendingFatalDamageContext = InContext;
		PendingFatalDamageContext.VictimActor = this;
		bHasPendingFatalDamageContext = true;
	}
}

FOnDeathSignature& AAuraCharacterBase::GetOnDeathDelegate()
{
	return OnDeathDelegate;
}

void AAuraCharacterBase::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());
	
	Weapon->SetSimulatePhysics(true);
	Weapon->SetEnableGravity(true);
	Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Weapon->AddImpulse(DeathImpulse * 0.1f, NAME_None, true);
	
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);
	
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dissolve();
	bDead = true;
	BurnDebuffComponent->Deactivate();
	StunDebuffComponent->Deactivate();
	OnDeathDelegate.Broadcast(this);
}

void AAuraCharacterBase::MulticastPlayGunFireFX_Implementation(
	const FVector_NetQuantize& MuzzleLocation,
	UParticleSystem* MuzzleFX, USoundBase* FireSound)
{
	// Cosmetic only — runs on the server and every client. The bullet handles impact + tracer FX;
	// this just plays the muzzle flash + fire sound. Dedicated servers bail early to skip FX.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (MuzzleFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, MuzzleFX, MuzzleLocation, GetActorRotation());
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation, GetActorRotation());
	}
}

void AAuraCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bIsStunned = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? 0.f : BaseWalkSpeed;
}

void AAuraCharacterBase::OnRep_Stunned()
{
	
}

void AAuraCharacterBase::OnRep_Burned()
{
}

void AAuraCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	bDead = !IsCombatAlive();

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay3NetworkProbe")) && !HasAuthority())
	{
		bDay3NetworkProbeStarted = true;
		const bool bClientStateMutationAccepted = TryBeginCombatDeath();
		FAuraCombatPolicySnapshot DefaultPolicy;
		const bool bClientPolicyAccepted = DefaultPolicy.IsTrustedFor(this);
		UE_LOG(LogAura, Display,
			TEXT("[Day3NetworkProbe][Client] ClientStateMutationAccepted=%d ClientPolicyMutationAccepted=%d"),
			bClientStateMutationAccepted,
			bClientPolicyAccepted);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay4DamageProbe")) && !HasAuthority() && !bDay4NetworkProbeStarted)
	{
		bDay4NetworkProbeStarted = true;
		UE_LOG(LogAura, Display, TEXT("[Day4DamageProbe][Client] InvalidDamageRequestSent=1"));
		ServerRoleBattleDay4InvalidDamageProbe();
	}
#endif

	if (HasAuthority())
	{
		DefaultCombatIdentity = BuildDefaultCombatIdentity();
		if (DefaultCombatIdentity.IsValid()
			&& (!CombatIdentityComponent || !CombatIdentityComponent->InitializeIdentity(DefaultCombatIdentity)))
		{
			UAuraCombatIdentityComponent::LogMissingIdentityOnce(this, TEXT("AAuraCharacterBase::BeginPlay"));
		}
	}
}

EAuraCombatLifeState AAuraCharacterBase::GetCombatLifeState() const
{
	return CombatStateComponent ? CombatStateComponent->GetLifeState() : EAuraCombatLifeState::Dead;
}

bool AAuraCharacterBase::IsCombatAlive() const
{
	return CombatStateComponent && CombatStateComponent->IsAlive();
}

bool AAuraCharacterBase::TryBeginCombatDeath()
{
	if (!HasAuthority() || !CombatStateComponent) return false;
	FAuraFatalDamageContext Context = bHasPendingFatalDamageContext ? PendingFatalDamageContext : FAuraFatalDamageContext();
	Context.VictimActor = this;
	if (!bHasPendingFatalDamageContext) Context.DeathImpulse = FVector::ZeroVector;
	bHasPendingFatalDamageContext = false;
	FAuraDeathEvent Event;
	return CombatStateComponent->TryEnterDying(Context, Event);
}

bool AAuraCharacterBase::MarkCombatReady()
{
	if (!HasAuthority() || !CombatStateComponent)
	{
		return false;
	}

	const bool bReady = CombatStateComponent->TryEnterAlive();
	if (bReady)
	{
		bDead = false;
		UE_LOG(LogAura, Log, TEXT("[CombatState][Server] Actor=%s transitioned to Alive after gameplay initialization."), *GetNameSafe(this));
		StartDay3NetworkProbe();
	}
	return bReady;
}

void AAuraCharacterBase::HandleCombatLifeStateChanged(EAuraCombatLifeState NewState)
{
	bDead = NewState != EAuraCombatLifeState::Alive;

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay3NetworkProbe")) && !HasAuthority()
		&& NewState != EAuraCombatLifeState::Alive)
	{
		UE_LOG(LogAura, Display, TEXT("[Day3NetworkProbe][Client] ReplicatedState=%s"),
			NewState == EAuraCombatLifeState::Dying ? TEXT("Dying")
			: NewState == EAuraCombatLifeState::Dead ? TEXT("Dead")
			: TEXT("Respawning"));
	}
#endif
}

void AAuraCharacterBase::StartDay3NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (bDay3NetworkProbeStarted || !HasAuthority() || !IsCombatAlive()
		|| !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay3NetworkProbe")))
	{
		return;
	}

	TWeakObjectPtr<AAuraCharacterBase> WeakThis(this);
	FTimerDelegate DelayedProbeDelegate;
	DelayedProbeDelegate.BindLambda([WeakThis]()
	{
		if (AAuraCharacterBase* Character = WeakThis.Get())
		{
			Character->ExecuteDay3NetworkProbe();
		}
	});
	GetWorldTimerManager().SetTimer(Day3NetworkProbeTimerHandle, DelayedProbeDelegate, 12.f, false);
#endif
}

void AAuraCharacterBase::ExecuteDay3NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (bDay3NetworkProbeStarted || !HasAuthority() || !IsCombatAlive()
		|| !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay3NetworkProbe")))
	{
		return;
	}

	const UAuraCombatIdentityComponent* IdentityComponent = GetCombatIdentityComponent();
	if (!IdentityComponent || !IdentityComponent->HasValidIdentity()
		|| !IdentityComponent->GetIdentity().FactionTag.MatchesTagExact(FAuraGameplayTags::Get().Faction_Player))
	{
		return;
	}

	bDay3NetworkProbeStarted = true;

	AActor* CivilianFixture = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (CivilianFixture)
	{
		UAuraCombatIdentityComponent* CivilianIdentity = NewObject<UAuraCombatIdentityComponent>(CivilianFixture);
		CivilianFixture->AddInstanceComponent(CivilianIdentity);
		CivilianIdentity->RegisterComponent();
		FAuraCombatIdentity Identity;
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		Identity.FactionTag = GameplayTags.Faction_Civilian;
		Identity.ControlTypeTag = GameplayTags.Control_CivilianAI;
		Identity.CombatProfileTag = GameplayTags.Combat_Civilian;
		Identity.DeathPolicyTag = GameplayTags.Death_PopulationRespawn;
		Identity.bTargetable = true;
		Identity.bCanBeDamaged = true;
		CivilianIdentity->InitializeIdentity(Identity);

		UAuraCombatStateComponent* CivilianState = NewObject<UAuraCombatStateComponent>(CivilianFixture);
		CivilianFixture->AddInstanceComponent(CivilianState);
		CivilianState->RegisterComponent();
		CivilianState->TryEnterAlive();

		FAuraCombatRuleContext Context;
		Context.TrustedWorldContext = this;
		const bool bCivilianAccepted = FAuraCombatRules::CanDamage(this, CivilianFixture, Context).bCanDamage;
		UE_LOG(LogAura, Display, TEXT("[Day3NetworkProbe][Server] CivilianDefaultPolicyDenied=%d"), !bCivilianAccepted);
		CivilianFixture->Destroy();
	}

	const bool bEnteredDying = CombatStateComponent && CombatStateComponent->TryEnterDying();
	UE_LOG(LogAura, Display, TEXT("[Day3NetworkProbe][Server] StateTransitionDying=%d"), bEnteredDying);

	TWeakObjectPtr<AAuraCharacterBase> WeakThis(this);
	FTimerDelegate CompleteDeathDelegate;
	CompleteDeathDelegate.BindLambda([WeakThis]()
	{
		if (AAuraCharacterBase* Character = WeakThis.Get())
		{
			const bool bEnteredDead = Character->CombatStateComponent && Character->CombatStateComponent->TryEnterDead();
			UE_LOG(LogAura, Display, TEXT("[Day3NetworkProbe][Server] StateTransitionDead=%d"), bEnteredDead);
		}
	});
	GetWorldTimerManager().SetTimer(Day3NetworkProbeTimerHandle, CompleteDeathDelegate, 0.75f, false);
#endif
}

void AAuraCharacterBase::ServerRoleBattleDay4InvalidDamageProbe_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay4DamageProbe")))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const UAuraAttributeSet* Attributes = Cast<UAuraAttributeSet>(GetAttributeSet());
	const float HealthBefore = Attributes ? Attributes->GetHealth() : 0.f;

	FDamageEffectParams InvalidParams;
	InvalidParams.WorldContextObject = this;
	InvalidParams.SourceAbilitySystemComponent = ASC;
	InvalidParams.TargetAbilitySystemComponent = ASC;
	InvalidParams.BaseDamage = 25.f;
	InvalidParams.AbilityLevel = 1.f;
	InvalidParams.DamageType = FAuraGameplayTags::Get().Damage_Physical;
	InvalidParams.AbilityTag = FAuraGameplayTags::Get().Abilities_Attack;
	InvalidParams.CombatRuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	InvalidParams.CombatRuleContext.TrustedWorldContext = this;
	InvalidParams.CombatRuleContext.SourceActor = this;
	InvalidParams.CombatRuleContext.TargetActor = this;

	const FGameplayEffectContextHandle Result = UAuraAbilitySystemLibrary::ApplyDamageEffect(InvalidParams);
	const UAuraAttributeSet* AttributesAfter = Cast<UAuraAttributeSet>(GetAttributeSet());
	const float HealthAfter = AttributesAfter ? AttributesAfter->GetHealth() : HealthBefore;
	const bool bHealthChanged = !FMath::IsNearlyEqual(HealthBefore, HealthAfter);
	UE_LOG(LogAura, Display,
		TEXT("[Day4DamageProbe][Server] ClientInvalidDamageRequestReceived=1 InvalidDamageRejected=%d HealthChanged=%d"),
		!Result.IsValid(), bHealthChanged);
#endif
}

FAuraCombatIdentity AAuraCharacterBase::BuildDefaultCombatIdentity() const
{
	return DefaultCombatIdentity;
}

FGameplayTag AAuraCharacterBase::GetRequiredRoleEntityType() const
{
	return FGameplayTag();
}

FGameplayTag AAuraCharacterBase::GetRequiredRoleControlType() const
{
	return FGameplayTag();
}

FVector AAuraCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Weapon) && IsValid(Weapon))
	{
		return Weapon->GetSocketLocation(WeaponTipSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftHand))
	{
		return GetMesh()->GetSocketLocation(LeftHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_RightHand))
	{
		return GetMesh()->GetSocketLocation(RightHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Tail))
	{
		return GetMesh()->GetSocketLocation(TailSocketName);
	}
	return FVector();
}

bool AAuraCharacterBase::IsDead_Implementation() const
{
	return CombatStateComponent ? !CombatStateComponent->IsAlive() : bDead;
}

AActor* AAuraCharacterBase::GetAvatar_Implementation()
{
	return this;
}

TArray<FTaggedMontage> AAuraCharacterBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

UNiagaraSystem* AAuraCharacterBase::GetBloodEffect_Implementation()
{
	return BloodEffect;
}

FTaggedMontage AAuraCharacterBase::GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag)
{
	for (FTaggedMontage TaggedMontage : AttackMontages)
	{
		if (TaggedMontage.MontageTag == MontageTag)
		{
			return TaggedMontage;
		}
	}
	return FTaggedMontage();
}

int32 AAuraCharacterBase::GetMinionCount_Implementation()
{
	return MinionCount;
}

void AAuraCharacterBase::IncremenetMinionCount_Implementation(int32 Amount)
{
	MinionCount += Amount;
}

ECharacterClass AAuraCharacterBase::GetCharacterClass_Implementation()
{
	return CharacterClass;
}

FOnASCRegistered& AAuraCharacterBase::GetOnASCRegisteredDelegate()
{
	return OnAscRegistered;
}

USkeletalMeshComponent* AAuraCharacterBase::GetWeapon_Implementation()
{
	return Weapon;
}

void AAuraCharacterBase::SetIsBeingShocked_Implementation(bool bInShock)
{
	bIsBeingShocked = bInShock;
}

bool AAuraCharacterBase::IsBeingShocked_Implementation() const
{
	return bIsBeingShocked;
}

bool AAuraCharacterBase::IsInAir_Implementation() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement != nullptr && Movement->IsFalling();
}

FOnDamageSignature& AAuraCharacterBase::GetOnDamageSignature()
{
	return OnDamageDelegate;
}

void AAuraCharacterBase::InitAbilityActorInfo()
{
}

void AAuraCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	UAuraAbilitySystemLibrary::AssignDefaultAttributeMagnitudes(SpecHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

bool AAuraCharacterBase::LoadAndApplySecondaryAttributes(bool bApplyZeroFallback) const
{
	FAuraAttributeDefaults Defaults;
	FString ConfigError;
	if (!FAuraGameplayConfig::GetAttributeDefaults(Defaults, ConfigError))
	{
		UE_LOG(LogAura, Warning, TEXT("[Attributes] GameplayEffects.json validation failed: %s"), *ConfigError);
		if (bApplyZeroFallback)
		{
			ApplyEffectToSelf(UAuraAttributeGameplayEffect::StaticClass(), 1.f);
		}
		return false;
	}

	FGameplayEffectContextHandle Context = GetAbilitySystemComponent()->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = GetAbilitySystemComponent()->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[Attributes] Failed to create the shared attribute GameplayEffect spec."));
		return false;
	}
	UAuraAbilitySystemLibrary::AssignDefaultAttributeMagnitudes(Spec);
	for (const TPair<FGameplayTag, float>& Pair : Defaults.Magnitudes)
	{
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Pair.Key, Pair.Value);
	}

	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	UE_LOG(LogAura, Log, TEXT("[Attributes] Secondary/vital/resistance applied from GameplayEffects.json."));

	// Initialize current Health/Mana to MaxHealth/MaxMana (MaxHealth/MaxMana set above).
	// The data-driven init no longer applies a separate DefaultVitalAttributes GE, so
	// without this the current Health/Mana stay at 0 and mana-cost abilities abort at CheckCost.
	UAuraAbilitySystemLibrary::TopOffVitalAttributes(GetAbilitySystemComponent(), this);
	return true;
}

void AAuraCharacterBase::InitializeDefaultAttributes() const
{
	// Legacy fallback: uses C++ GEs with zero primary magnitudes.
	ApplyEffectToSelf(UAuraAttributeGameplayEffect::StaticClass(), 1.f);
	LoadAndApplySecondaryAttributes(true);
}

bool AAuraCharacterBase::InitializeDefaultAttributesForRole(FName InRole, const FRoleDefaultInfo& RoleDefinition) const
{
	if (!GetAbilitySystemComponent())
	{
		return false;
	}

	// Safety net: if the role config left all primary attribute values unset, keep the legacy
	// BP-set DefaultPrimaryAttributes GE so the character isn't zeroed out.
	if (RoleDefinition.Strength == 0.f && RoleDefinition.Intelligence == 0.f && RoleDefinition.Resilience == 0.f && RoleDefinition.Vigor == 0.f)
	{
		UE_LOG(LogAura, Error, TEXT("[Role][Attributes] %s: Role='%s' has no staged primary attributes."),
			*GetNameSafe(this), *InRole.ToString());
		return false;
	}

	UE_LOG(LogAura, Log, TEXT("[Role][Attributes] %s: Role='%s' primary via SetByCaller (Str=%.1f Int=%.1f Res=%.1f Vig=%.1f)."),
		*GetNameSafe(this), *InRole.ToString(), RoleDefinition.Strength, RoleDefinition.Intelligence, RoleDefinition.Resilience, RoleDefinition.Vigor);

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	// Primary attributes via the C++ SetByCaller GE, magnitudes from the role config.
	FGameplayEffectContextHandle PrimaryContext = GetAbilitySystemComponent()->MakeEffectContext();
	PrimaryContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle PrimarySpec = GetAbilitySystemComponent()->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, PrimaryContext);
	if (!PrimarySpec.IsValid())
	{
		return false;
	}
	UAuraAbilitySystemLibrary::AssignDefaultAttributeMagnitudes(PrimarySpec);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Strength, RoleDefinition.Strength);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Intelligence, RoleDefinition.Intelligence);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Resilience, RoleDefinition.Resilience);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Vigor, RoleDefinition.Vigor);

	// Secondary + Vital + Resistance via the same C++ GE, magnitudes from GameplayEffects.json.
	if (!LoadAndApplySecondaryAttributes(false))
	{
		return false;
	}

	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*PrimarySpec.Data.Get());
	return true;
}

void AAuraCharacterBase::Dissolve()
{
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}
	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}
