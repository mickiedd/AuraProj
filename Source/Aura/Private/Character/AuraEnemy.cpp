// Copyright Druid Mechanics


#include "Character/AuraEnemy.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "Aura/Aura.h"
#include "UI/Widget/AuraUserWidget.h"
#include "AuraGameplayTags.h"
#include "AI/AuraAIController.h"
#include "AI/AuraBehaviacAgentComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Aura/AuraLogChannels.h"

AAuraEnemy::AAuraEnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	AbilitySystemComponent = CreateDefaultSubobject<UAuraAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	AttributeSet = CreateDefaultSubobject<UAuraAttributeSet>("AttributeSet");

	HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

	GetMesh()->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	GetMesh()->MarkRenderStateDirty();
	Weapon->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	Weapon->MarkRenderStateDirty();
	
	BaseWalkSpeed = 250.f;
	Tags.AddUnique(FName("Enemy"));
	AIControllerClass = AAuraAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Bind the BehaviorU test behavior tree (BT_TestEnemy.xml). The component's
	// constructor defaults AutoLoadXMLFilePath to the test tree, so simply
	// instantiating it is enough — no per-enemy Blueprint wiring required.
	BehaviorUAgentComponent = CreateDefaultSubobject<UAuraBehaviacAgentComponent>(TEXT("BehaviorUAgentComponent"));
}

void AAuraEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UE_LOG(LogAura, Log, TEXT("[EnemyAI] PossessedBy: Enemy=%s Controller=%s HasAuthority=%s BT=%s"),
		*GetNameSafe(this),
		*GetNameSafe(NewController),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(BehaviorTree));

	if (!HasAuthority()) return;
	if (!IsValid(BehaviorTree) || !IsValid(BehaviorTree->BlackboardAsset))
	{
		UE_LOG(LogAura, Warning, TEXT("[EnemyAI] Missing BehaviorTree or BlackboardAsset on %s. AI will remain idle."), *GetNameSafe(this));
		return;
	}

	AuraAIController = Cast<AAuraAIController>(NewController);
	if (!IsValid(AuraAIController) || !IsValid(AuraAIController->GetBlackboardComponent()))
	{
		UE_LOG(LogAura, Warning, TEXT("[EnemyAI] Invalid AI controller for %s. Controller=%s"), *GetNameSafe(this), *GetNameSafe(NewController));
		return;
	}

	AuraAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	AuraAIController->RunBehaviorTree(BehaviorTree);
	AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), false);
	AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);
	UE_LOG(LogAura, Log, TEXT("[EnemyAI] BT initialized: Enemy=%s Controller=%s Blackboard=%s RangedAttacker=%s"),
		*GetNameSafe(this),
		*GetNameSafe(AuraAIController),
		*GetNameSafe(BehaviorTree->BlackboardAsset),
		(CharacterClass != ECharacterClass::Warrior) ? TEXT("true") : TEXT("false"));
}

void AAuraEnemy::HighlightActor_Implementation()
{
	GetMesh()->SetRenderCustomDepth(true);
	Weapon->SetRenderCustomDepth(true);
}

void AAuraEnemy::UnHighlightActor_Implementation()
{
	GetMesh()->SetRenderCustomDepth(false);
	Weapon->SetRenderCustomDepth(false);
}

void AAuraEnemy::SetMoveToLocation_Implementation(FVector& OutDestination)
{
	// Do not change OutDestination
}

int32 AAuraEnemy::GetPlayerLevel_Implementation()
{
	return Level;
}

void AAuraEnemy::Die(const FVector& DeathImpulse)
{
	SetLifeSpan(LifeSpan);
	if (AuraAIController) AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);
	SpawnLoot();
	Super::Die(DeathImpulse);
}

void AAuraEnemy::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	CombatTarget = InCombatTarget;
}

AActor* AAuraEnemy::GetCombatTarget_Implementation() const
{
	return CombatTarget;
}

void AAuraEnemy::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAura, Log, TEXT("[EnemyAI] BeginPlay: Enemy=%s HasAuthority=%s Controller=%s AIControllerClass=%s BT=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetController()),
		*GetNameSafe(AIControllerClass),
		*GetNameSafe(BehaviorTree));

	if (HasAuthority() && !IsValid(GetController()))
	{
		// Table/deferred spawns can occasionally miss auto possession; enforce on server.
		UE_LOG(LogAura, Warning, TEXT("[EnemyAI] BeginPlay missing controller, calling SpawnDefaultController: Enemy=%s"), *GetNameSafe(this));
		SpawnDefaultController();
		UE_LOG(LogAura, Log, TEXT("[EnemyAI] SpawnDefaultController result: Enemy=%s ControllerNow=%s"),
			*GetNameSafe(this), *GetNameSafe(GetController()));
	}

	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	InitAbilityActorInfo();
	if (HasAuthority())
	{
		UAuraAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);	
	}

	
	if (UAuraUserWidget* AuraUserWidget = Cast<UAuraUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		AuraUserWidget->SetWidgetController(this);
	}
	
	if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);
		
		AbilitySystemComponent->RegisterGameplayTagEvent(FAuraGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&AAuraEnemy::HitReactTagChanged
		);

		OnHealthChanged.Broadcast(AuraAS->GetHealth());
		OnMaxHealthChanged.Broadcast(AuraAS->GetMaxHealth());
	}
	
}

void AAuraEnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
	if (AuraAIController && AuraAIController->GetBlackboardComponent())
	{
		AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
	}
}

void AAuraEnemy::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent)->AbilityActorInfoSet();
	AbilitySystemComponent->RegisterGameplayTagEvent(FAuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAuraEnemy::StunTagChanged);


	if (HasAuthority())
	{
		InitializeDefaultAttributes();		
	}
	OnAscRegistered.Broadcast(AbilitySystemComponent);
}

void AAuraEnemy::InitializeDefaultAttributes() const
{
	UAuraAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

void AAuraEnemy::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	Super::StunTagChanged(CallbackTag, NewCount);
	
	if (AuraAIController && AuraAIController->GetBlackboardComponent())
	{
		AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("Stunned"), bIsStunned);
	}
}
