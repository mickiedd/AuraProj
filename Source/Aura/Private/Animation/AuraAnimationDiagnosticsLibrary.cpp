// Copyright Druid Mechanics

#include "Animation/AuraAnimationDiagnosticsLibrary.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraCharacterBase.h"
#include "Character/AuraCivilian.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"

namespace AuraAnimationDiagnosticsPrivate
{
	// Weak keys avoid retaining destroyed pawns or reusing state at recycled addresses.
	TMap<TWeakObjectPtr<APawn>, double> LastLogTimes;
}

void UAuraAnimationDiagnosticsLibrary::LogCivilianAnimationState(
	APawn* PawnOwner,
	UCharacterMovementComponent* CachedMovement,
	float BlueprintGroundSpeed)
{
	// Do not inspect animation state or mutate diagnostic bookkeeping unless explicitly enabled.
	if (!UE_LOG_ACTIVE(LogAuraAnimationDiagnostics, Verbose))
	{
		AuraAnimationDiagnosticsPrivate::LastLogTimes.Reset();
		return;
	}

	AAuraCharacterBase* Character = Cast<AAuraCharacterBase>(PawnOwner);
	if (!Character || (!Character->GetCharacterRole().IsEqual(FName(TEXT("Civilian")))
		&& !Character->IsA(AAuraCivilian::StaticClass())))
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const TWeakObjectPtr<APawn> PawnKey(PawnOwner);
	const double* LastLogTime = AuraAnimationDiagnosticsPrivate::LastLogTimes.Find(PawnKey);
	if (LastLogTime && Now - *LastLogTime < 1.0)
	{
		return;
	}
	for (auto It = AuraAnimationDiagnosticsPrivate::LastLogTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	AuraAnimationDiagnosticsPrivate::LastLogTimes.Add(PawnKey, Now);

	const FVector ActualVelocity = Character->GetVelocity();
	const float ActualGroundSpeed = ActualVelocity.Size2D();
	UCharacterMovementComponent* ActualMovement = Character->GetCharacterMovement();
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const FName StateMachineName(TEXT("Main States"));
	const int32 StateMachineIndex = AnimInstance ? AnimInstance->GetStateMachineIndex(StateMachineName) : INDEX_NONE;
	const FAnimNode_StateMachine* StateMachine = AnimInstance && StateMachineIndex != INDEX_NONE
		? AnimInstance->GetStateMachineInstance(StateMachineIndex) : nullptr;
	const FBakedAnimationStateMachine* StateMachineDescription = AnimInstance
		? AnimInstance->GetStateMachineInstanceDesc(StateMachineName) : nullptr;
	const FName CurrentState = StateMachine && StateMachineDescription
		? StateMachine->GetCurrentStateName() : NAME_None;
	const int32 CurrentStateIndex = StateMachine ? StateMachine->GetCurrentState() : INDEX_NONE;
	const float RelevantAnimLength = AnimInstance && StateMachineIndex != INDEX_NONE && CurrentStateIndex != INDEX_NONE
		? AnimInstance->GetRelevantAnimLength(StateMachineIndex, CurrentStateIndex) : 0.f;
	const float RelevantAnimTime = AnimInstance && StateMachineIndex != INDEX_NONE && CurrentStateIndex != INDEX_NONE
		? AnimInstance->GetRelevantAnimTime(StateMachineIndex, CurrentStateIndex) : 0.f;
	const bool bCachedMovementValid = IsValid(CachedMovement);
	const bool bMovementMatches = bCachedMovementValid && CachedMovement == ActualMovement;
	const int32 MovementMode = ActualMovement ? static_cast<int32>(ActualMovement->MovementMode) : INDEX_NONE;
	const bool bMovingOnGround = ActualMovement && ActualMovement->IsMovingOnGround();
	const bool bActuallyInAir = ActualMovement && ActualMovement->IsFalling();
	const bool bDead = Character->IsDead_Implementation();

	UE_LOG(LogAuraAnimationDiagnostics, Verbose,
		TEXT("[CivilianAnimDiag] actor=%s authority=%d role=%s pawnClass=%s cachedMovement=%d movementMatches=%d "
			"actualMovement=%d mode=%d movingOnGround=%d actuallyInAir=%d dead=%d maxWalkSpeed=%.1f "
			"actualVelocity=%s actualGroundSpeed=%.2f blueprintGroundSpeed=%.2f mesh=%s animClass=%s animInstance=%s state=%s relevantAnimLength=%.3f relevantAnimTime=%.3f montage=%s"),
		*GetNameSafe(Character), Character->HasAuthority() ? 1 : 0, *Character->GetCharacterRole().ToString(),
		*GetNameSafe(Character->GetClass()), bCachedMovementValid ? 1 : 0, bMovementMatches ? 1 : 0,
		IsValid(ActualMovement) ? 1 : 0, MovementMode, bMovingOnGround ? 1 : 0, bActuallyInAir ? 1 : 0,
		bDead ? 1 : 0, ActualMovement ? ActualMovement->MaxWalkSpeed : 0.f,
		*ActualVelocity.ToCompactString(), ActualGroundSpeed, BlueprintGroundSpeed,
		Mesh && Mesh->GetSkeletalMeshAsset() ? *GetNameSafe(Mesh->GetSkeletalMeshAsset()) : TEXT("None"),
		Mesh && Mesh->GetAnimInstance() ? *GetNameSafe(Mesh->GetAnimInstance()->GetClass()) : TEXT("None"),
		IsValid(AnimInstance) ? TEXT("valid") : TEXT("None"),
		*CurrentState.ToString(),
		RelevantAnimLength, RelevantAnimTime,
		AnimInstance && AnimInstance->IsAnyMontagePlaying() ? TEXT("playing") : TEXT("none"));
}
