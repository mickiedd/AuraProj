// Copyright Druid Mechanics


#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemComponent.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UTargetDataUnderMouse* UTargetDataUnderMouse::CreateTargetDataUnderMouse(UGameplayAbility* OwningAbility)
{
	UTargetDataUnderMouse* MyObj = NewAbilityTask<UTargetDataUnderMouse>(OwningAbility);
	return MyObj;
}

void UTargetDataUnderMouse::Activate()
{
	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
	UE_LOG(LogAura, Warning, TEXT("[TargetData] Activate: LocallyControlled=%d IsNetAuth=%d PredKey=%s Handle=%s"),
		bIsLocallyControlled,
		(int32)Ability->GetCurrentActorInfo()->IsNetAuthority(),
		*GetActivationPredictionKey().ToString(),
		*GetAbilitySpecHandle().ToString());
	if (bIsLocallyControlled)
	{
		SendMouseCursorData();
	}
	else
	{
		const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
		AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UTargetDataUnderMouse::OnTargetDataReplicatedCallback);
		const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);
		UE_LOG(LogAura, Warning, TEXT("[TargetData] Server path: PredKey=%s Handle=%s CalledDelegateImmediately=%d"),
			*ActivationPredictionKey.ToString(), *SpecHandle.ToString(), bCalledDelegate);
		if (!bCalledDelegate)
		{
			SetWaitingOnRemotePlayerData();
		}
	}
}

void UTargetDataUnderMouse::SendMouseCursorData()
{
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());
	
	APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	FHitResult CursorHit;
	PC->GetHitResultUnderCursor(ECC_Target, false, CursorHit);

	// Headless / no-cursor contexts (e.g. the AutoTest stress harness under -nullrhi) never
	// produce a blocking cursor hit, which would leave the target data pointing at the world
	// origin and make projectile spells fire toward (0,0,0). Fall back to a random spot in a
	// forward-facing cone in front of the controlled pawn so the ability still gets a sensible
	// target. This only triggers when there is no blocking hit — normal play (cursor over
	// ground/walls/enemies) is unaffected; a player aiming at open sky now fires forward
	// instead of at the origin.
	if (!CursorHit.bBlockingHit && PC)
	{
		if (APawn* ControlledPawn = PC->GetPawn())
		{
			const FVector PawnLoc = ControlledPawn->GetActorLocation();
			const float RandomYaw = FMath::FRandRange(-45.f, 45.f);
			const float Distance = FMath::FRandRange(1500.f, 3000.f);
			const FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(RandomYaw));
			const FVector FallbackTarget = PawnLoc + YawQuat.RotateVector(ControlledPawn->GetActorForwardVector()) * Distance;

			CursorHit.bBlockingHit = true;
			CursorHit.ImpactPoint = FallbackTarget;
			CursorHit.Location = FallbackTarget;
			CursorHit.TraceStart = PawnLoc;
			CursorHit.TraceEnd = FallbackTarget;
			// Deliberately leave HitObjectHandle empty. A real cursor hit over an enemy sets the
			// hit actor so projectile spells (FireBolt) home onto it. Setting it to the controlled
			// pawn here would make the bolt home back onto — and explode on — the player, since the
			// player implements ICombatInterface. With no actor, SpawnProjectiles homes toward the
			// ImpactPoint instead, so the bolt lands at the forward point.
			UE_LOG(LogAura, Log, TEXT("[TargetData] No cursor hit — falling back to forward point %s."),
				*CursorHit.ImpactPoint.ToString());
		}
	}

	FGameplayAbilityTargetDataHandle DataHandle;
	FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
	Data->HitResult = CursorHit;
	DataHandle.Add(Data);
	
	AbilitySystemComponent->ServerSetReplicatedTargetData(
		GetAbilitySpecHandle(),
		GetActivationPredictionKey(),
		DataHandle,
		FGameplayTag(),
		AbilitySystemComponent->ScopedPredictionKey);

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		ValidData.Broadcast(DataHandle);
	}
}

void UTargetDataUnderMouse::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	const bool bShouldBroadcast = ShouldBroadcastAbilityTaskDelegates();
	UE_LOG(LogAura, Warning, TEXT("[TargetData] OnTargetDataReplicatedCallback: ShouldBroadcast=%d DataNum=%d"),
		bShouldBroadcast, DataHandle.Num());
	if (bShouldBroadcast)
	{
		ValidData.Broadcast(DataHandle);
	}
}
