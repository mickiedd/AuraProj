// Copyright Druid Mechanics

#include "Commands/AuraCreateCrunchPresentationCommandlet.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AuraCreateCrunchPresentationPrivate
{
	constexpr TCHAR SourceMeshPath[] = TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch.Crunch");
	constexpr TCHAR SourceSkeletonPath[] = TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch_Skeleton.Crunch_Skeleton");
	constexpr TCHAR SourceSequencePaths[4][256] = {
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_01.Ability_Combo_01"),
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_02.Ability_Combo_02"),
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_03.Ability_Combo_03"),
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_04.Ability_Combo_04")
	};
	constexpr TCHAR SourceLocomotionSequencePaths[2][256] = {
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Idle_Combat.Idle_Combat"),
		TEXT("/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Jog_Fwd.Jog_Fwd")
	};
	constexpr TCHAR TargetMeshPackage[] = TEXT("/Game/Assets/Characters/Crunch/Meshes/SM_CrunchV4");
	constexpr TCHAR TargetSkeletonPackage[] = TEXT("/Game/Assets/Characters/Crunch/Meshes/Crunch_SkeletonV4");
	constexpr TCHAR TargetSequencePackages[4][256] = {
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/Ability_Combo_01V4"),
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/Ability_Combo_02V4"),
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/Ability_Combo_03V4"),
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/Ability_Combo_04V4")
	};
	constexpr TCHAR TargetLocomotionSequencePackages[2][256] = {
		TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Idle_CombatV4"),
		TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Jog_FwdV4")
	};
	constexpr TCHAR TargetMontagePackage[] = TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchComboV4");
	constexpr TCHAR NotifyClassPath[] = TEXT("/Game/Blueprints/AnimNotifies/AN_MontageEvent.AN_MontageEvent_C");
	constexpr TCHAR OpenTagName[] = TEXT("Event.Montage.Crunch.Combo.Window.Open");
	constexpr TCHAR DamageTagName[] = TEXT("Event.Montage.Crunch.Combo.Damage");
	constexpr TCHAR CloseTagName[] = TEXT("Event.Montage.Crunch.Combo.Window.Close");

	bool SavePackageAsset(UPackage* Package, UObject* Asset, const FString& PackageName)
	{
		if (!Package || !Asset) return false;
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}

	UAnimNotify* CreateEventNotify(UObject* Outer, UClass* NotifyClass, const FGameplayTag& EventTag, const FName NotifyName)
	{
		UAnimNotify* Notify = NewObject<UAnimNotify>(Outer, NotifyClass, NotifyName, RF_Transactional);
		if (!Notify) return nullptr;
		FStructProperty* EventTagProperty = FindFProperty<FStructProperty>(NotifyClass, TEXT("EventTag"));
		if (!EventTagProperty || EventTagProperty->Struct != TBaseStructure<FGameplayTag>::Get()) return nullptr;
		*EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Notify) = EventTag;
		return Notify;
	}

	bool AddEvent(UAnimMontage* Montage, UClass* NotifyClass, const FGameplayTag& EventTag, float Time, FName Name)
	{
		FAnimNotifyEvent Event;
		Event.Notify = CreateEventNotify(Montage, NotifyClass, EventTag, Name);
		if (!Event.Notify) return false;
	Event.NotifyName = Name;
	Event.TrackIndex = 0;
	Event.bTriggerOnDedicatedServer = true;
	// The montage slot track is installed after the translated events are
	// collected. Preserve the authored absolute time now, then relink against
	// the final slot track once all segments exist.
	Event.SetTime(Time);
	Montage->Notifies.Add(MoveTemp(Event));
	return true;
	}
}

int32 UAuraCreateCrunchPresentationCommandlet::Main(const FString& Params)
{
	using namespace AuraCreateCrunchPresentationPrivate;
	const bool bLocomotionOnly = FParse::Param(*Params, TEXT("LocomotionOnly"));
	USkeleton* SourceSkeleton = LoadObject<USkeleton>(nullptr, SourceSkeletonPath);
	USkeletalMesh* SourceMesh = bLocomotionOnly ? nullptr : LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
	if (!SourceSkeleton || (!bLocomotionOnly && !SourceMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Source mesh or skeleton unavailable."));
		return 1;
	}

	USkeleton* TargetSkeleton = nullptr;
	USkeletalMesh* TargetMesh = nullptr;
	if (bLocomotionOnly)
	{
		TargetSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Assets/Characters/Crunch/Meshes/Crunch_SkeletonV4.Crunch_SkeletonV4"));
		if (!TargetSkeleton)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Existing target skeleton is required for LocomotionOnly."));
			return 1;
		}
	}
	else
	{
		UPackage* SkeletonPackage = CreatePackage(TargetSkeletonPackage);
		TargetSkeleton = DuplicateObject<USkeleton>(SourceSkeleton, SkeletonPackage, TEXT("Crunch_SkeletonV4"));
		if (!SavePackageAsset(SkeletonPackage, TargetSkeleton, TargetSkeletonPackage))
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to save target skeleton."));
			return 1;
		}

		UPackage* MeshPackage = CreatePackage(TargetMeshPackage);
		TargetMesh = DuplicateObject<USkeletalMesh>(SourceMesh, MeshPackage, TEXT("SM_CrunchV4"));
		if (!TargetMesh)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to duplicate target mesh."));
			return 1;
		}
		TargetMesh->SetSkeleton(TargetSkeleton);
		if (!SavePackageAsset(MeshPackage, TargetMesh, TargetMeshPackage))
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to save target mesh."));
			return 1;
		}
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UAnimSequence* SourceSequence = LoadObject<UAnimSequence>(nullptr, SourceLocomotionSequencePaths[Index]);
		if (!SourceSequence)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Missing source locomotion sequence %d."), Index + 1);
			return 1;
		}
		UPackage* SequencePackage = CreatePackage(TargetLocomotionSequencePackages[Index]);
		const FName TargetName = Index == 0 ? FName(TEXT("Idle_CombatV4")) : FName(TEXT("Jog_FwdV4"));
		UAnimSequence* TargetSequence = DuplicateObject<UAnimSequence>(SourceSequence, SequencePackage, TargetName);
		if (!TargetSequence)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to duplicate source locomotion sequence %d."), Index + 1);
			return 1;
		}
		TargetSequence->SetSkeleton(TargetSkeleton);
		// Locomotion pose selection is owned by the Aura AnimBP. Source gameplay
		// notifies are not part of that presentation-only contract.
		TargetSequence->Notifies.Empty();
		if (!SavePackageAsset(SequencePackage, TargetSequence, TargetLocomotionSequencePackages[Index]))
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to save target locomotion sequence %d."), Index + 1);
			return 1;
		}
	}
	if (bLocomotionOnly)
	{
		UE_LOG(LogTemp, Display, TEXT("[CrunchPresentation] PASS LocomotionOnly=1 Skeleton=%s Locomotion=2"),
			*TargetSkeleton->GetPathName());
		return 0;
	}

	TArray<UAnimSequence*> TargetSequences;
	TargetSequences.Reserve(4);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UAnimSequence* SourceSequence = LoadObject<UAnimSequence>(nullptr, SourceSequencePaths[Index]);
		if (!SourceSequence)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Missing source sequence %d."), Index + 1);
			return 1;
		}
		UPackage* SequencePackage = CreatePackage(TargetSequencePackages[Index]);
		const FString Name = FString::Printf(TEXT("Ability_Combo_%02dV4"), Index + 1);
		UAnimSequence* TargetSequence = DuplicateObject<UAnimSequence>(SourceSequence, SequencePackage, *Name);
		if (!TargetSequence)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to duplicate source sequence %d."), Index + 1);
			return 1;
		}
		TargetSequence->SetSkeleton(TargetSkeleton);
		// Source hit/VFX notifies reference Crunch-only classes. The translated
		// montage owns the authoritative Aura event notifies instead.
		TargetSequence->Notifies.Empty();
		if (!SavePackageAsset(SequencePackage, TargetSequence, TargetSequencePackages[Index]))
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to save target sequence %d."), Index + 1);
			return 1;
		}
		TargetSequences.Add(TargetSequence);
	}

	UClass* NotifyClass = LoadClass<UAnimNotify>(nullptr, NotifyClassPath);
	if (!NotifyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Aura montage notify class unavailable."));
		return 1;
	}
	UPackage* MontagePackage = CreatePackage(TargetMontagePackage);
	UAnimMontage* Montage = NewObject<UAnimMontage>(MontagePackage, TEXT("AM_CrunchComboV4"), RF_Public | RF_Standalone);
	Montage->SetSkeleton(TargetSkeleton);
	// NewObject<UAnimMontage> carries a DefaultSlot track in editor builds;
	// replace it so the translated montage has one authoritative slot track.
	Montage->SlotAnimTracks.Empty();
	FSlotAnimationTrack SlotTrack;
	// Aura's animation-instance contract exposes DefaultSlot; use the shared
	// slot while retaining the Crunch skeleton and source timing.
	SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
	float TotalLength = 0.f;
	for (const UAnimSequence* Sequence : TargetSequences)
	{
		TotalLength += Sequence ? Sequence->GetPlayLength() : 0.f;
	}
	Montage->GetController().SetPlayLength(TotalLength, false);
	// The final close notify must fire before the montage's automatic blend-out.
	// A zero trigger offset keeps the blend-out at sequence end instead of the
	// default 0.25s early boundary, which would skip Combo04's close event.
	Montage->BlendOutTriggerTime = 0.f;
	float SectionStart = 0.f;
	for (int32 Index = 0; Index < TargetSequences.Num(); ++Index)
	{
		const float SequenceLength = TargetSequences[Index]->GetPlayLength();
		FAnimSegment Segment;
		Segment.SetAnimReference(TargetSequences[Index], true);
		Segment.StartPos = SectionStart;
		Segment.AnimStartTime = 0.f;
		Segment.AnimEndTime = SequenceLength;
		Segment.AnimPlayRate = 1.f;
		Segment.LoopingCount = 1;
		SlotTrack.AnimTrack.AnimSegments.Add(MoveTemp(Segment));

		FCompositeSection Section;
		Section.SectionName = FName(*FString::Printf(TEXT("Combo%02d"), Index + 1));
		Section.NextSectionName = NAME_None;
		Section.Link(Montage, SectionStart, 0);
		Montage->CompositeSections.Add(MoveTemp(Section));

		const float OpenTime = SectionStart + SequenceLength * 0.35f;
		const float DamageTime = SectionStart + SequenceLength * 0.55f;
		const float CloseTime = SectionStart + SequenceLength * 0.75f;
		if (!AddEvent(Montage, NotifyClass, FGameplayTag::RequestGameplayTag(OpenTagName), OpenTime, FName(*FString::Printf(TEXT("CrunchComboOpen%02d"), Index + 1)))
			|| !AddEvent(Montage, NotifyClass, FGameplayTag::RequestGameplayTag(DamageTagName), DamageTime, FName(*FString::Printf(TEXT("CrunchComboDamage%02d"), Index + 1)))
			|| !AddEvent(Montage, NotifyClass, FGameplayTag::RequestGameplayTag(CloseTagName), CloseTime, FName(*FString::Printf(TEXT("CrunchComboClose%02d"), Index + 1))))
		{
			UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to add translated montage events."));
			return 1;
		}
		SectionStart += SequenceLength;
	}
	Montage->SlotAnimTracks.Add(MoveTemp(SlotTrack));
	// Link sections and notify events only after the slot track exists. Calling
	// Link against an empty montage silently leaves every element at time zero.
	float RelinkSectionStart = 0.f;
	for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
	{
		Montage->CompositeSections[Index].Link(Montage, RelinkSectionStart, 0);
		RelinkSectionStart += TargetSequences[Index]->GetPlayLength();
	}
	for (FAnimNotifyEvent& Event : Montage->Notifies)
	{
		Event.Link(Montage, Event.GetTriggerTime(), 0);
	}
	Montage->SortNotifies();
	if (!SavePackageAsset(MontagePackage, Montage, TargetMontagePackage))
	{
		UE_LOG(LogTemp, Error, TEXT("[CrunchPresentation] Failed to save target montage."));
		return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("[CrunchPresentation] PASS Mesh=%s Skeleton=%s Locomotion=2 Montage=%s Sections=%d Notifies=%d Length=%.3f"),
		*TargetMesh->GetPathName(), *TargetSkeleton->GetPathName(), *Montage->GetPathName(), Montage->CompositeSections.Num(), Montage->Notifies.Num(), Montage->GetPlayLength());
	return 0;
}
