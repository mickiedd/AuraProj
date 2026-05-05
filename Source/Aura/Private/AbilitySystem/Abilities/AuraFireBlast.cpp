// Copyright Druid Mechanics


#include "AbilitySystem/Abilities/AuraFireBlast.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Actor/AuraFireBall.h"
#include "Aura/AuraLogChannels.h"

FString UAuraFireBlast::GetDescription(int32 Level)
{
	const int32 ScaledDamage = Damage.GetValueAtLevel(Level);
	const float ManaCost = FMath::Abs(GetManaCost(Level));
	const float Cooldown = GetCooldown(Level);
	return FString::Printf(TEXT(
			// Title
			"<Title>FIRE BLAST</>\n\n"

			// Level
			"<Small>Level: </><Level>%d</>\n"
			// ManaCost
			"<Small>ManaCost: </><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>Cooldown: </><Cooldown>%.1f</>\n\n"

			// Number of Fire Balls
			"<Default>Launches %d </>"
			"<Default>fire balls in all directions, each coming back and </>"
			"<Default>exploding upon return, causing </>"

			// Damage
			"<Damage>%d</><Default> radial fire damage with"
			" a chance to burn</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			NumFireBalls,
			ScaledDamage);
}

FString UAuraFireBlast::GetNextLevelDescription(int32 Level)
{
	const int32 ScaledDamage = Damage.GetValueAtLevel(Level);
	const float ManaCost = FMath::Abs(GetManaCost(Level));
	const float Cooldown = GetCooldown(Level);
	return FString::Printf(TEXT(
			// Title
			"<Title>NEXT LEVEL:</>\n\n"

			// Level
			"<Small>Level: </><Level>%d</>\n"
			// ManaCost
			"<Small>ManaCost: </><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>Cooldown: </><Cooldown>%.1f</>\n\n"

			// Number of Fire Balls
			"<Default>Launches %d </>"
			"<Default>fire balls in all directions, each coming back and </>"
			"<Default>exploding upon return, causing </>"

			// Damage
			"<Damage>%d</><Default> radial fire damage with"
			" a chance to burn</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			NumFireBalls,
			ScaledDamage);
}

TArray<AAuraFireBall*> UAuraFireBlast::SpawnFireBalls()
{
	TArray<AAuraFireBall*> FireBalls;
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		UE_LOG(LogAura, Warning, TEXT("[FireBlast] SpawnFireBalls aborted: AvatarActor invalid"));
		return FireBalls;
	}

	if (!AvatarActor->HasAuthority())
	{
		UE_LOG(LogAura, Verbose, TEXT("[FireBlast] SpawnFireBalls skipped on non-authority Avatar=%s"), *GetNameSafe(AvatarActor));
		return FireBalls;
	}

	if (!FireBallClass)
	{
		UE_LOG(LogAura, Warning, TEXT("[FireBlast] SpawnFireBalls aborted: FireBallClass is null Ability=%s"), *GetNameSafe(this));
		return FireBalls;
	}

	const FVector Forward = AvatarActor->GetActorForwardVector();
	const FVector Location = AvatarActor->GetActorLocation();
	TArray<FRotator> Rotators = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 360.f, NumFireBalls);

	UE_LOG(LogAura, Log, TEXT("[FireBlast] Spawning fireballs: Ability=%s Avatar=%s Num=%d Location=%s"),
		*GetNameSafe(this), *GetNameSafe(AvatarActor), NumFireBalls, *Location.ToCompactString());

	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);

	for (const FRotator& Rotator : Rotators)
	{
		FTransform SpawnTransform;
		SpawnTransform.SetLocation(Location);
		SpawnTransform.SetRotation(Rotator.Quaternion());
		
		AAuraFireBall* FireBall = GetWorld()->SpawnActorDeferred<AAuraFireBall>(
			FireBallClass,
			SpawnTransform,
			GetOwningActorFromActorInfo(),
			InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!IsValid(FireBall))
		{
			UE_LOG(LogAura, Warning, TEXT("[FireBlast] SpawnActorDeferred failed for one fireball. Transform=%s"), *SpawnTransform.GetLocation().ToCompactString());
			continue;
		}
		
		FireBall->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
		FireBall->ReturnToActor = AvatarActor;
		FireBall->SetOwner(AvatarActor);

		FireBall->ExplosionDamageParams = MakeDamageEffectParamsFromClassDefaults();
		FireBall->SetOwner(AvatarActor);

		FireBalls.Add(FireBall);

		FireBall->FinishSpawning(SpawnTransform);
		UE_LOG(LogAura, Verbose, TEXT("[FireBlast] FireBall spawned: Actor=%s ReturnToActor=%s"),
			*GetNameSafe(FireBall), *GetNameSafe(FireBall->ReturnToActor));
	}
	
	return FireBalls;
}
