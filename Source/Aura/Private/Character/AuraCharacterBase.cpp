// Copyright Druid Mechanics


#include "Character/AuraCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "AbilitySystem/Passive/PassiveNiagaraComponent.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "AuraAttributeGameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

#include "AuraAbilityGraph/Public/AbilityDefinition.h"

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

	// Weapon mesh + socket. The weapon is tied to the role's LMB skill: a role with no LMB
	// ability (Info.DefaultLMBAbility / Info.DefaultLMBAbilityDefinition empty) holds NO weapon,
	// so clear the mesh and skip attach.
	// A role with an LMB ability equips its configured weapon (if specified) on the role's socket.
	if (Info.DefaultLMBAbility || Info.DefaultLMBAbilityDefinition)
	{
		UE_LOG(LogAura, Log, TEXT("[Role][Apply] %s: equipping weapon mesh=%s socket=%s"),
			*GetNameSafe(this), *GetNameSafe(Info.WeaponMesh.Get()), *Info.WeaponSocketName.ToString());
		if (Info.WeaponMesh)
		{
			Weapon->SetSkeletalMeshAsset(Info.WeaponMesh);
		}
		if (!Info.WeaponSocketName.IsNone() && Weapon->GetAttachSocketName() != Info.WeaponSocketName)
		{
			Weapon->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, Info.WeaponSocketName);
		}
	}
	else
	{
		// No LMB skill → no weapon. Clear the mesh so this role spawns empty-handed rather than
		// inheriting the BP-default (Aura) staff.
		UE_LOG(LogAura, Log, TEXT("[Role][Apply] %s: clearing weapon (no LMB skill)"), *GetNameSafe(this));
		Weapon->SetSkeletalMeshAsset(nullptr);
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

	// Data-driven ability definitions (granted as UAuraDataAbility with SourceObject = Definition).
	if (Info.StartupAbilityDefinitions.Num() > 0)
	{
		StartupAbilityDefinitionObjects.SetNum(Info.StartupAbilityDefinitions.Num());
		for (int32 i = 0; i < Info.StartupAbilityDefinitions.Num(); ++i)
		{
			StartupAbilityDefinitionObjects[i] = Cast<UObject>(Info.StartupAbilityDefinitions[i].Get());
		}
	}
	if (Info.StartupPassiveAbilityDefinitions.Num() > 0)
	{
		StartupPassiveAbilityDefinitionObjects.SetNum(Info.StartupPassiveAbilityDefinitions.Num());
		for (int32 i = 0; i < Info.StartupPassiveAbilityDefinitions.Num(); ++i)
		{
			StartupPassiveAbilityDefinitionObjects[i] = Cast<UObject>(Info.StartupPassiveAbilityDefinitions[i].Get());
		}
	}

	// LMB default skill: prefer data-driven definition, fall back to legacy class path.
	const FGameplayTag LMBInputTag = FAuraGameplayTags::Get().InputTag_LMB;
	StartupAbilities.RemoveAll([&LMBInputTag](const TSubclassOf<UGameplayAbility>& AbilityClass)
	{
		if (const UAuraGameplayAbility* DefaultObj = Cast<UAuraGameplayAbility>(AbilityClass.GetDefaultObject()))
		{
			return DefaultObj->StartupInputTag.MatchesTagExact(LMBInputTag);
		}
		return false;
	});

	if (Info.DefaultLMBAbilityDefinition)
	{
		DefaultLMBAbilityDefinitionObject = Cast<UObject>(Info.DefaultLMBAbilityDefinition.Get());
	}
	else if (Info.DefaultLMBAbility)
	{
		StartupAbilities.AddUnique(Info.DefaultLMBAbility);
	}

	UE_LOG(LogAura, Log, TEXT("[Role][Apply] %s: applied Role='%s' (mesh=%s anim=%s weapon=%s)."),
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

void AAuraCharacterBase::LoadAndApplySecondaryAttributes() const
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	// Load GameplayEffects.json for secondary/vital/resistance default values.
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("GameplayEffects.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogAura, Warning, TEXT("[Attributes] Failed to load GameplayEffects.json: %s — using zero defaults."), *ConfigPath);
		ApplyEffectToSelf(UAuraAttributeGameplayEffect::StaticClass(), 1.f);
		return;
	}

	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[Attributes] Failed to parse GameplayEffects.json — using zero defaults."));
		ApplyEffectToSelf(UAuraAttributeGameplayEffect::StaticClass(), 1.f);
		return;
	}

	// Build the spec and assign SetByCaller magnitudes from JSON.
	FGameplayEffectContextHandle Context = GetAbilitySystemComponent()->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = GetAbilitySystemComponent()->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, Context);

	auto AssignFromJson = [&Spec](const TSharedPtr<FJsonObject>& Obj, FGameplayTag Tag, const FString& FieldName)
	{
		if (Obj.IsValid() && Obj->HasField(FieldName))
		{
			const float Value = static_cast<float>(Obj->GetNumberField(FieldName));
			UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Tag, Value);
		}
	};

	const TSharedPtr<FJsonObject>& Secondary = RootObj->GetObjectField(TEXT("secondaryAttributes"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_Armor, TEXT("Armor"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_ArmorPenetration, TEXT("ArmorPenetration"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_BlockChance, TEXT("BlockChance"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitChance, TEXT("CriticalHitChance"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitDamage, TEXT("CriticalHitDamage"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitResistance, TEXT("CriticalHitResistance"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_HealthRegeneration, TEXT("HealthRegeneration"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_ManaRegeneration, TEXT("ManaRegeneration"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxHealth, TEXT("MaxHealth"));
	AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxMana, TEXT("MaxMana"));

	const TSharedPtr<FJsonObject>& Resistances = RootObj->GetObjectField(TEXT("resistances"));
	AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Fire, TEXT("Fire"));
	AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Lightning, TEXT("Lightning"));
	AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Arcane, TEXT("Arcane"));
	AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Physical, TEXT("Physical"));

	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	UE_LOG(LogAura, Log, TEXT("[Attributes] Secondary/vital/resistance applied from GameplayEffects.json."));

	// Initialize current Health/Mana to MaxHealth/MaxMana (MaxHealth/MaxMana set above).
	// The data-driven init no longer applies a separate DefaultVitalAttributes GE, so
	// without this the current Health/Mana stay at 0 and mana-cost abilities abort at CheckCost.
	UAuraAbilitySystemLibrary::TopOffVitalAttributes(GetAbilitySystemComponent(), this);
}

void AAuraCharacterBase::InitializeDefaultAttributes() const
{
	// Legacy fallback: uses C++ GEs with zero primary magnitudes.
	ApplyEffectToSelf(UAuraAttributeGameplayEffect::StaticClass(), 1.f);
	LoadAndApplySecondaryAttributes();
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

	// Primary attributes via the C++ SetByCaller GE, magnitudes from the role config.
	FGameplayEffectContextHandle PrimaryContext = GetAbilitySystemComponent()->MakeEffectContext();
	PrimaryContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle PrimarySpec = GetAbilitySystemComponent()->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, PrimaryContext);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Strength, Info.Strength);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Intelligence, Info.Intelligence);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Resilience, Info.Resilience);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(PrimarySpec, GameplayTags.Attributes_Primary_Vigor, Info.Vigor);

	// Secondary + Vital + Resistance via the same C++ GE, magnitudes from GameplayEffects.json.
	LoadAndApplySecondaryAttributes();

	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*PrimarySpec.Data.Get());
}

void AAuraCharacterBase::AddCharacterAbilities()
{
	UAuraAbilitySystemComponent* AuraASC = CastChecked<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	if (!HasAuthority()) return;

	AuraASC->AddCharacterAbilities(StartupAbilities);
	AuraASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
	TArray<UAuraAbilityDefinition*> DataAbilityDefs;
	for (const TObjectPtr<UObject>& Obj : StartupAbilityDefinitionObjects)
	{
		DataAbilityDefs.Add(Cast<UAuraAbilityDefinition>(Obj.Get()));
	}
	if (DefaultLMBAbilityDefinitionObject)
	{
		DataAbilityDefs.Add(Cast<UAuraAbilityDefinition>(DefaultLMBAbilityDefinitionObject.Get()));
	}
	AuraASC->AddCharacterDataAbilities(DataAbilityDefs);
	TArray<UAuraAbilityDefinition*> DataPassiveDefs;
	for (const TObjectPtr<UObject>& Obj : StartupPassiveAbilityDefinitionObjects)
	{
		DataPassiveDefs.Add(Cast<UAuraAbilityDefinition>(Obj.Get()));
	}
	AuraASC->AddCharacterDataPassiveAbilities(DataPassiveDefs);
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