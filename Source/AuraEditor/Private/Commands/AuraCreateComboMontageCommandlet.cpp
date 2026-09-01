// Copyright Druid Mechanics

#include "Commands/AuraCreateComboMontageCommandlet.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AuraCreateComboMontagePrivate
{
	constexpr TCHAR SourcePath[] = TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun.AM_FireGun");
	constexpr TCHAR OutputPackageName[] = TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_CrunchCombo_Prototype_RuntimeV2");
	constexpr TCHAR OutputObjectName[] = TEXT("AM_CrunchCombo_Prototype_RuntimeV2");
	constexpr TCHAR NotifyClassPath[] = TEXT("/Game/Blueprints/AnimNotifies/AN_MontageEvent.AN_MontageEvent_C");

	UAnimNotify* CreateEventNotify(UObject* Outer, UClass* NotifyClass, const FGameplayTag& EventTag, const FName NotifyName)
	{
		UAnimNotify* Notify = NewObject<UAnimNotify>(Outer, NotifyClass, NotifyName, RF_Transactional);
		if (!Notify)
		{
			return nullptr;
		}

		FStructProperty* EventTagProperty = FindFProperty<FStructProperty>(NotifyClass, TEXT("EventTag"));
		if (!EventTagProperty || EventTagProperty->Struct != TBaseStructure<FGameplayTag>::Get())
		{
			return nullptr;
		}
		*EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Notify) = EventTag;
		return Notify;
	}

	bool AddEvent(UAnimMontage* Montage, UClass* NotifyClass, const FGameplayTag& EventTag, const float Time, const FName NotifyName, const int32 TrackIndex)
	{
		FAnimNotifyEvent Event;
		Event.Notify = CreateEventNotify(Montage, NotifyClass, EventTag, NotifyName);
		if (!Event.Notify)
		{
			return false;
		}
		Event.NotifyName = NotifyName;
		Event.TrackIndex = TrackIndex;
		Event.bTriggerOnDedicatedServer = true;
		Event.Link(Montage, Time, 0);
		Montage->Notifies.Add(MoveTemp(Event));
		return true;
	}
}

int32 UAuraCreateComboMontageCommandlet::Main(const FString& Params)
{
	UAnimMontage* SourceMontage = LoadObject<UAnimMontage>(nullptr, AuraCreateComboMontagePrivate::SourcePath);
	if (!SourceMontage || SourceMontage->SlotAnimTracks.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Source montage is unavailable: %s"), AuraCreateComboMontagePrivate::SourcePath);
		return 1;
	}

	UClass* NotifyClass = LoadClass<UAnimNotify>(nullptr, AuraCreateComboMontagePrivate::NotifyClassPath);
	if (!NotifyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] AN_MontageEvent class is unavailable: %s"), AuraCreateComboMontagePrivate::NotifyClassPath);
		return 1;
	}

	UPackage* Package = CreatePackage(AuraCreateComboMontagePrivate::OutputPackageName);
	if (!Package)
	{
		return 1;
	}
	UAnimMontage* Montage = DuplicateObject<UAnimMontage>(SourceMontage, Package, AuraCreateComboMontagePrivate::OutputObjectName);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Failed to duplicate source montage."));
		return 1;
	}

	const float SourceLength = SourceMontage->GetPlayLength();
	if (SourceLength <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Source montage has no playable length."));
		return 1;
	}

	Montage->SlotAnimTracks.Empty();
	for (const FSlotAnimationTrack& SourceSlot : SourceMontage->SlotAnimTracks)
	{
		FSlotAnimationTrack NewSlot = SourceSlot;
		NewSlot.AnimTrack.AnimSegments.Empty();
		for (int32 ComboIndex = 0; ComboIndex < 4; ++ComboIndex)
		{
			for (const FAnimSegment& SourceSegment : SourceSlot.AnimTrack.AnimSegments)
			{
				FAnimSegment NewSegment = SourceSegment;
				NewSegment.StartPos = SourceSegment.StartPos + (SourceLength * ComboIndex);
				NewSlot.AnimTrack.AnimSegments.Add(NewSegment);
			}
		}
		Montage->SlotAnimTracks.Add(MoveTemp(NewSlot));
	}
	const float CalculatedLength = Montage->CalculateSequenceLength();
	if (CalculatedLength <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Duplicated montage has no playable segment length."));
		return 1;
	}
	Montage->GetController().SetPlayLength(CalculatedLength, false);

	Montage->CompositeSections.Empty();
	Montage->Notifies.Empty();
	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Damage"));
	const FGameplayTag OpenTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Window.Open"));
	const FGameplayTag CloseTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Window.Close"));
	for (int32 ComboIndex = 0; ComboIndex < 4; ++ComboIndex)
	{
		const float SectionStart = SourceLength * ComboIndex;
		const FName SectionName = FName(*FString::Printf(TEXT("Combo%02d"), ComboIndex + 1));
		FCompositeSection Section;
		Section.SectionName = SectionName;
		// Sections intentionally have no authored successor. The ability queues the
		// next section only after a discrete input press inside the open window.
		Section.NextSectionName = NAME_None;
		Section.Link(Montage, SectionStart, 0);
		Montage->CompositeSections.Add(MoveTemp(Section));

		const float OpenTime = SectionStart + SourceLength * 0.35f;
		const float DamageTime = SectionStart + SourceLength * 0.55f;
		const float CloseTime = SectionStart + SourceLength * 0.75f;
		if (!AuraCreateComboMontagePrivate::AddEvent(Montage, NotifyClass, OpenTag, OpenTime, FName(*FString::Printf(TEXT("CrunchComboOpen%02d"), ComboIndex + 1)), 0)
			|| !AuraCreateComboMontagePrivate::AddEvent(Montage, NotifyClass, DamageTag, DamageTime, FName(*FString::Printf(TEXT("CrunchComboDamage%02d"), ComboIndex + 1)), 0)
			|| !AuraCreateComboMontagePrivate::AddEvent(Montage, NotifyClass, CloseTag, CloseTime, FName(*FString::Printf(TEXT("CrunchComboClose%02d"), ComboIndex + 1)), 0))
		{
			UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Failed to create a gameplay-event notify."));
			return 1;
		}
	}

	Montage->SortNotifies();
	Montage->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Montage);
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		AuraCreateComboMontagePrivate::OutputPackageName,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Montage, *PackageFilename, SaveArgs))
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraComboFixture] Failed to save %s"), *PackageFilename);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[AuraComboFixture] Created %s from %s with four sections and dedicated-server events."),
		AuraCreateComboMontagePrivate::OutputPackageName, AuraCreateComboMontagePrivate::SourcePath);
	return 0;
}
