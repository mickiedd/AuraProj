
#include "AuraAbilityTypes.h"

bool FAuraGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;
	uint32 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (bReplicateInstigator && Instigator.IsValid())
		{
			RepBits |= 1 << 0;
		}
		if (bReplicateEffectCauser && EffectCauser.IsValid() )
		{
			RepBits |= 1 << 1;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2;
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
		if (bIsBlockedHit)
		{
			RepBits |= 1 << 7;
		}
		if (bIsCriticalHit)
		{
			RepBits |= 1 << 8;
		}
		if (bIsSuccessfulDebuff)
		{
			RepBits |= 1 << 9;
		}
		if (DebuffDamage > 0.f)
		{
			RepBits |= 1 << 10;
		}
		if (DebuffDuration > 0.f)
		{
			RepBits |= 1 << 11;
		}
		if (DebuffFrequency > 0.f)
		{
			RepBits |= 1 << 12;
		}
		if (DamageType.IsValid())
		{
			RepBits |= 1 << 13;
		}
		if (!DeathImpulse.IsZero())
		{
			RepBits |= 1 << 14;
		}
		if (!KnockbackForce.IsZero())
		{
			RepBits |= 1 << 15;
		}
		if (bIsRadialDamage)
		{
			RepBits |= 1 << 16;

			if (RadialDamageInnerRadius > 0.f)
			{
				RepBits |= 1 << 17;
			}
			if (RadialDamageOuterRadius > 0.f)
			{
				RepBits |= 1 << 18;
			}
			if (!RadialDamageOrigin.IsZero())
			{
				RepBits |= 1 << 19;
			}
		}
		if (AbilityTag.IsValid())
		{
			RepBits |= 1 << 20;
		}
		if (!SourceRoleId.IsNone())
		{
			RepBits |= 1 << 21;
		}
		if (SourceController.IsValid())
		{
			RepBits |= 1 << 22;
		}
		if (SourcePlayerState.IsValid())
		{
			RepBits |= 1 << 23;
		}
		if (!BattleZoneId.IsNone())
		{
			RepBits |= 1 << 24;
		}
		if (!BattleEventId.IsNone())
		{
			RepBits |= 1 << 25;
		}
		
	}

	if (Ar.IsLoading())
	{
		Instigator.Reset();
		EffectCauser.Reset();
		AbilityCDO.Reset();
		SourceObject.Reset();
		Actors.Reset();
		HitResult.Reset();
		bHasWorldOrigin = false;
		WorldOrigin = FVector::ZeroVector;
		bIsBlockedHit = false;
		bIsCriticalHit = false;
		bIsSuccessfulDebuff = false;
		DebuffDamage = 0.f;
		DebuffDuration = 0.f;
		DebuffFrequency = 0.f;
		DamageType.Reset();
		AbilityTag = FGameplayTag();
		SourceRoleId = NAME_None;
		SourceController.Reset();
		SourcePlayerState.Reset();
		BattleZoneId = NAME_None;
		BattleEventId = NAME_None;
		DeathImpulse = FVector::ZeroVector;
		KnockbackForce = FVector::ZeroVector;
		bIsRadialDamage = false;
		RadialDamageInnerRadius = 0.f;
		RadialDamageOuterRadius = 0.f;
		RadialDamageOrigin = FVector::ZeroVector;
	}

	Ar.SerializeBits(&RepBits, 26);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}
	if (RepBits & (1 << 7))
	{
		Ar << bIsBlockedHit;
	}
	if (RepBits & (1 << 8))
	{
		Ar << bIsCriticalHit;
	}
	if (RepBits & (1 << 9))
	{
		Ar << bIsSuccessfulDebuff;
	}
	if (RepBits & (1 << 10))
	{
		Ar << DebuffDamage;
	}
	if (RepBits & (1 << 11))
	{
		Ar << DebuffDuration;
	}
	if (RepBits & (1 << 12))
	{
		Ar << DebuffFrequency;
	}
	if (RepBits & (1 << 13))
	{
		if (Ar.IsLoading())
		{
			if (!DamageType.IsValid())
			{
				DamageType = TSharedPtr<FGameplayTag>(new FGameplayTag());
			}
		}
		DamageType->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 14))
	{
		DeathImpulse.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 15))
	{
		KnockbackForce.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 16))
	{
		Ar << bIsRadialDamage;
		
		if (RepBits & (1 << 17))
		{
			Ar << RadialDamageInnerRadius;
		}
		if (RepBits & (1 << 18))
		{
			Ar << RadialDamageOuterRadius;
		}
	if (RepBits & (1 << 19))
		{
			RadialDamageOrigin.NetSerialize(Ar, Map, bOutSuccess);
		}
	}
	if (RepBits & (1 << 20))
	{
		AbilityTag.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 21))
	{
		Ar << SourceRoleId;
	}
	if (RepBits & (1 << 22))
	{
		Ar << SourceController;
	}
	if (RepBits & (1 << 23))
	{
		Ar << SourcePlayerState;
	}
	if (RepBits & (1 << 24))
	{
		Ar << BattleZoneId;
	}
	if (RepBits & (1 << 25))
	{
		Ar << BattleEventId;
	}
	

	if (Ar.IsLoading() && bOutSuccess)
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}
	
	return bOutSuccess;
}
