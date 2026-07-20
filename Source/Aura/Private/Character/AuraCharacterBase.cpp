// Copyright Druid Mechanics


#include "Character/AuraCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "AbilitySystem/Passive/PassiveNiagaraComponent.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AAuraCharacterBase::AAuraCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	
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

void AAuraCharacterBase::ApplyRole(FName InRole)
{
	if (CharacterRole == InRole)
	{
		UE_LOG(LogAura, Log, TEXT("[Role][Apply] %s: Role='%s' already applied (no-op)."),
			*GetNameSafe(this), *InRole.ToString());
		return;
	}

	URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	if (RoleInfo == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("[Role][Apply] %s: URoleInfo not configured on the GameMode; cannot apply Role='%s'."),
			*GetNameSafe(this), *InRole.ToString());
		return;
	}
	if (!RoleInfo->RoleInformation.Contains(InRole))
	{
		UE_LOG(LogAura, Warning, TEXT("[Role][Apply] %s: Role='%s' not found in RoleConfig.json; leaving BP defaults."),
			*GetNameSafe(this), *InRole.ToString());
		return;
	}

	const FRoleDefaultInfo Info = RoleInfo->GetRoleDefaultInfo(InRole);

	// Body mesh + anim blueprint. Only set when the role specifies one; for mesh, skip the
	// redundant set when it already matches (avoids a needless re-init). Anim is (re)set
	// unconditionally when specified — re-setting the same class is a harmless one-time cost.
	if (Info.SkeletalMesh && GetMesh()->GetSkeletalMeshAsset() != Info.SkeletalMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(Info.SkeletalMesh);
	}
	if (Info.AnimBlueprintClass)
	{
		GetMesh()->SetAnimInstanceClass(Info.AnimBlueprintClass);
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}

	// Weapon mesh + socket. Re-attach when the role's weapon socket differs from the current one.
	if (Info.WeaponMesh)
	{
		Weapon->SetSkeletalMeshAsset(Info.WeaponMesh);
	}
	if (!Info.WeaponSocketName.IsNone() && Weapon->GetAttachSocketName() != Info.WeaponSocketName)
	{
		Weapon->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, Info.WeaponSocketName);
	}

	// Combat sockets are skeleton-dependent. Only override when the role specifies a name, so an
	// unspecified socket keeps the BP default (GetCombatSocketLocation still resolves correctly).
	if (!Info.WeaponTipSocketName.IsNone()) WeaponTipSocketName = Info.WeaponTipSocketName;
	if (!Info.LeftHandSocketName.IsNone()) LeftHandSocketName = Info.LeftHandSocketName;
	if (!Info.RightHandSocketName.IsNone()) RightHandSocketName = Info.RightHandSocketName;
	if (!Info.TailSocketName.IsNone()) TailSocketName = Info.TailSocketName;

	// Death / dissolve VFX. Only override when the role specifies an asset, so an unspecified
	// field keeps the BP default rather than clearing it to null.
	if (Info.DissolveMaterialInstance) DissolveMaterialInstance = Info.DissolveMaterialInstance;
	if (Info.WeaponDissolveMaterialInstance) WeaponDissolveMaterialInstance = Info.WeaponDissolveMaterialInstance;
	if (Info.BloodEffect) BloodEffect = Info.BloodEffect;
	if (Info.DeathSound) DeathSound = Info.DeathSound;

	// Gameplay: copy startup abilities so AddCharacterAbilities() grants role-specific skills.
	// (Primary attributes come from the role's numeric values via InitializeDefaultAttributesForRole.)
	// Only overwrite when the role actually lists abilities — an empty JSON array means "leave the
	// BP defaults", so an under-filled role (e.g. a test default) doesn't strip all abilities.
	if (Info.StartupAbilities.Num() > 0)
	{
		StartupAbilities = Info.StartupAbilities;
	}
	if (Info.StartupPassiveAbilities.Num() > 0)
	{
		StartupPassiveAbilities = Info.StartupPassiveAbilities;
	}

	UE_LOG(LogAura, Log, TEXT("[Role][Apply] %s: applied Role='%s' (mesh=%s anim=%s weapon=%s, abilities=%d passive=%d)."),
		*GetNameSafe(this), *InRole.ToString(),
		*GetNameSafe(Info.SkeletalMesh.Get()), *GetNameSafe(Info.AnimBlueprintClass.Get()),
		*GetNameSafe(Info.WeaponMesh.Get()), Info.StartupAbilities.Num(), Info.StartupPassiveAbilities.Num());

	CharacterRole = InRole;
}

UAnimMontage* AAuraCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void AAuraCharacterBase::Die(const FVector& DeathImpulse)
{
	Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
	MulticastHandleDeath(DeathImpulse);
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
	return bDead;
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
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void AAuraCharacterBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void AAuraCharacterBase::InitializeDefaultAttributesForRole(FName InRole) const
{
	URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	UCharacterClassInfo* CharacterClassInfo = UAuraAbilitySystemLibrary::GetCharacterClassInfo(this);
	if (RoleInfo == nullptr || CharacterClassInfo == nullptr || !RoleInfo->RoleInformation.Contains(InRole))
	{
		// No role data available / role not in config: fall back to the BP-set DefaultPrimaryAttributes path.
		InitializeDefaultAttributes();
		return;
	}

	const FRoleDefaultInfo Info = RoleInfo->GetRoleDefaultInfo(InRole);

	// Safety net: if the role config left all primary attribute values unset, keep the legacy
	// BP-set DefaultPrimaryAttributes GE so the character isn't zeroed out.
	if (Info.Strength == 0.f && Info.Intelligence == 0.f && Info.Resilience == 0.f && Info.Vigor == 0.f)
	{
		UE_LOG(LogAura, Warning, TEXT("[Role][Attributes] %s: Role='%s' has no attribute values in RoleConfig.json; falling back to BP DefaultPrimaryAttributes."), *GetNameSafe(this), *InRole.ToString());
		InitializeDefaultAttributes();
		return;
	}

	UE_LOG(LogAura, Log, TEXT("[Role][Attributes] %s: Role='%s' primary via SetByCaller (Str=%.1f Int=%.1f Res=%.1f Vig=%.1f)."),
		*GetNameSafe(this), *InRole.ToString(), Info.Strength, Info.Intelligence, Info.Resilience, Info.Vigor);

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	// Primary attributes via the shared SetByCaller GE, magnitudes from the role config.
	FGameplayEffectContextHandle PrimaryContext = GetAbilitySystemComponent()->MakeEffectContext();
	PrimaryContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle PrimarySpec = GetAbilitySystemComponent()->MakeOutgoingSpec(CharacterClassInfo->PrimaryAttributes_SetByCaller, 1.f, PrimaryContext);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Strength, Info.Strength);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Intelligence, Info.Intelligence);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Resilience, Info.Resilience);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Vigor, Info.Vigor);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*PrimarySpec.Data.Get());

	// Secondary + Vital stay shared (BP-set members), matching InitializeDefaultAttributes().
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void AAuraCharacterBase::AddCharacterAbilities()
{
	UAuraAbilitySystemComponent* AuraASC = CastChecked<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	if (!HasAuthority()) return;

	AuraASC->AddCharacterAbilities(StartupAbilities);
	AuraASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
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

