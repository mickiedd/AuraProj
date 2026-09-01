// Copyright Druid Mechanics

#include "Commands/AuraValidateComboMontageCommandlet.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "GameplayTagContainer.h"

namespace AuraValidateComboMontagePrivate
{
	constexpr TCHAR MontagePath[] = TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_CrunchCombo_Prototype_RuntimeV2.AM_CrunchCombo_Prototype_RuntimeV2");
	constexpr TCHAR AbilityClassPath[] = TEXT("/Script/Aura.AuraMeleeAttack");
	constexpr TCHAR SkeletonPath[] = TEXT("/Game/Assets/Characters/Aura/SK_Aura");
	constexpr TCHAR OpenTagName[] = TEXT("Event.Montage.Crunch.Combo.Window.Open");
	constexpr TCHAR DamageTagName[] = TEXT("Event.Montage.Crunch.Combo.Damage");
	constexpr TCHAR CloseTagName[] = TEXT("Event.Montage.Crunch.Combo.Window.Close");

	bool ReadEventTag(const FAnimNotifyEvent& Event, FGameplayTag& OutTag)
	{
		if (!Event.Notify)
		{
			return false;
		}
		const UClass* NotifyClass = Event.Notify->GetClass();
		const FStructProperty* EventTagProperty = FindFProperty<FStructProperty>(NotifyClass, TEXT("EventTag"));
		if (!EventTagProperty || EventTagProperty->Struct != TBaseStructure<FGameplayTag>::Get())
		{
			return false;
		}
		OutTag = *EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Event.Notify);
		return OutTag.IsValid();
	}
}

int32 UAuraValidateComboMontageCommandlet::Main(const FString& Params)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, AuraValidateComboMontagePrivate::MontagePath);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Missing montage: %s"), AuraValidateComboMontagePrivate::MontagePath);
		return 1;
	}
	UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, AuraValidateComboMontagePrivate::AbilityClassPath);
	const bool bAbilityClassValid = AbilityClass && AbilityClass->IsChildOf(UAuraMeleeAttack::StaticClass());

	bool bValid = bAbilityClassValid && Montage->GetSkeleton() && Montage->GetSkeleton()->GetPathName().StartsWith(AuraValidateComboMontagePrivate::SkeletonPath);
	bValid = bValid && Montage->GetPlayLength() > 2.f;
	const TArray<FName> ExpectedSections = { FName("Combo01"), FName("Combo02"), FName("Combo03"), FName("Combo04") };
	if (Montage->CompositeSections.Num() != ExpectedSections.Num())
	{
		bValid = false;
	}
	for (int32 Index = 0; Index < ExpectedSections.Num() && Index < Montage->CompositeSections.Num(); ++Index)
	{
		const FCompositeSection& Section = Montage->CompositeSections[Index];
		bValid = bValid && Section.SectionName == ExpectedSections[Index] && Section.NextSectionName.IsNone();
	}

	int32 OpenCount = 0;
	int32 DamageCount = 0;
	int32 CloseCount = 0;
	const FGameplayTag OpenTag = FGameplayTag::RequestGameplayTag(AuraValidateComboMontagePrivate::OpenTagName);
	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(AuraValidateComboMontagePrivate::DamageTagName);
	const FGameplayTag CloseTag = FGameplayTag::RequestGameplayTag(AuraValidateComboMontagePrivate::CloseTagName);
	TArray<float> OpenTimes;
	TArray<float> DamageTimes;
	TArray<float> CloseTimes;
	OpenTimes.Init(-1.f, ExpectedSections.Num());
	DamageTimes.Init(-1.f, ExpectedSections.Num());
	CloseTimes.Init(-1.f, ExpectedSections.Num());
	bool bEmbeddedNotifiesClean = true;
	int32 EmbeddedNotifyCount = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		FGameplayTag EventTag;
		const bool bHasTag = AuraValidateComboMontagePrivate::ReadEventTag(Event, EventTag);
		bValid = bValid && bHasTag && Event.bTriggerOnDedicatedServer;
		if (!bHasTag)
		{
			continue;
		}
		int32 SectionIndex = INDEX_NONE;
		const float EventTime = Event.GetTriggerTime();
		for (int32 CandidateIndex = 0; CandidateIndex < ExpectedSections.Num(); ++CandidateIndex)
		{
			float SectionStart = 0.f;
			float SectionEnd = 0.f;
			Montage->GetSectionStartAndEndTime(CandidateIndex, SectionStart, SectionEnd);
			if (EventTime > SectionStart + KINDA_SMALL_NUMBER && EventTime < SectionEnd - KINDA_SMALL_NUMBER)
			{
				SectionIndex = CandidateIndex;
				break;
			}
		}
		bValid = bValid && SectionIndex != INDEX_NONE;
		if (EventTag.MatchesTagExact(OpenTag))
		{
			++OpenCount;
			if (SectionIndex != INDEX_NONE && OpenTimes[SectionIndex] >= 0.f) bValid = false;
			if (SectionIndex != INDEX_NONE) OpenTimes[SectionIndex] = EventTime;
		}
		if (EventTag.MatchesTagExact(DamageTag))
		{
			++DamageCount;
			if (SectionIndex != INDEX_NONE && DamageTimes[SectionIndex] >= 0.f) bValid = false;
			if (SectionIndex != INDEX_NONE) DamageTimes[SectionIndex] = EventTime;
		}
		if (EventTag.MatchesTagExact(CloseTag))
		{
			++CloseCount;
			if (SectionIndex != INDEX_NONE && CloseTimes[SectionIndex] >= 0.f) bValid = false;
			if (SectionIndex != INDEX_NONE) CloseTimes[SectionIndex] = EventTime;
		}
	}
	bValid = bValid && Montage->Notifies.Num() == 12 && OpenCount == 4 && DamageCount == 4 && CloseCount == 4;
	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
		{
			if (const UAnimSequenceBase* Sequence = Segment.GetAnimReference())
			{
				EmbeddedNotifyCount += Sequence->Notifies.Num();
				bEmbeddedNotifiesClean = bEmbeddedNotifiesClean && Sequence->Notifies.IsEmpty();
				UE_LOG(LogTemp, Display, TEXT("[AuraComboFixture] Source sequence %s embedded notifies=%d"),
					*Sequence->GetPathName(), Sequence->Notifies.Num());
				for (const FAnimNotifyEvent& EmbeddedEvent : Sequence->Notifies)
				{
					FGameplayTag EmbeddedTag;
					const bool bEmbeddedTag = AuraValidateComboMontagePrivate::ReadEventTag(EmbeddedEvent, EmbeddedTag);
					UE_LOG(LogTemp, Display, TEXT("[AuraComboFixture] Embedded notify time=%.3f class=%s tag=%s"),
						EmbeddedEvent.GetTriggerTime(), *GetNameSafe(EmbeddedEvent.Notify),
						bEmbeddedTag ? *EmbeddedTag.ToString() : TEXT("None"));
				}
			}
		}
	}
	bValid = bValid && bEmbeddedNotifiesClean;
	for (int32 Index = 0; Index < ExpectedSections.Num(); ++Index)
	{
		float SectionStart = 0.f;
		float SectionEnd = 0.f;
		Montage->GetSectionStartAndEndTime(Index, SectionStart, SectionEnd);
		bValid = bValid && OpenTimes[Index] >= 0.f && OpenTimes[Index] < DamageTimes[Index] && DamageTimes[Index] < CloseTimes[Index];
		UE_LOG(LogTemp, Display, TEXT("[AuraComboFixture] %s timing | start=%.3f open=%.3f damage=%.3f close=%.3f end=%.3f window=%.3f"),
			*ExpectedSections[Index].ToString(), SectionStart, OpenTimes[Index], DamageTimes[Index], CloseTimes[Index], SectionEnd, CloseTimes[Index] - OpenTimes[Index]);
	}

	UE_LOG(LogTemp, Display,
		TEXT("[AuraComboFixture] Validation %s | AbilityClass=%s Length=%.3f Sections=%d Notifies=%d EmbeddedNotifies=%d Open=%d Damage=%d Close=%d Skeleton=%s"),
		bValid ? TEXT("PASSED") : TEXT("FAILED"),
		bAbilityClassValid ? TEXT("loaded") : TEXT("missing"),
		Montage->GetPlayLength(),
		Montage->CompositeSections.Num(),
		Montage->Notifies.Num(),
		EmbeddedNotifyCount,
		OpenCount,
		DamageCount,
		CloseCount,
		Montage->GetSkeleton() ? *Montage->GetSkeleton()->GetPathName() : TEXT("None"));
	return bValid ? 0 : 1;
}
