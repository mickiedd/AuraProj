// Copyright Druid Mechanics

#include "Commands/AuraCreateFireBlastMontageCommandlet.h"

#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace AuraCreateFireBlastMontagePrivate
{
	constexpr TCHAR SourcePath[] = TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt.AM_Cast_FireBolt");
	constexpr TCHAR OutputPackageName[] = TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBlast");
	constexpr TCHAR OutputObjectName[] = TEXT("AM_Cast_FireBlast");
	constexpr TCHAR NotifyClassPath[] = TEXT("/Game/Blueprints/AnimNotifies/AN_MontageEvent.AN_MontageEvent_C");

	UAnimNotify* CreateEventNotify(UObject* Outer, UClass* NotifyClass, const FGameplayTag& EventTag)
	{
		UAnimNotify* Notify = NewObject<UAnimNotify>(Outer, NotifyClass, TEXT("FireBlastRelease"), RF_Transactional);
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
}

int32 UAuraCreateFireBlastMontageCommandlet::Main(const FString& Params)
{
	UAnimMontage* SourceMontage = LoadObject<UAnimMontage>(nullptr, AuraCreateFireBlastMontagePrivate::SourcePath);
	if (!SourceMontage || SourceMontage->SlotAnimTracks.Num() == 0 || SourceMontage->GetPlayLength() <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraFireBlastAsset] Source montage is unavailable or empty: %s"), AuraCreateFireBlastMontagePrivate::SourcePath);
		return 1;
	}

	UClass* NotifyClass = LoadClass<UAnimNotify>(nullptr, AuraCreateFireBlastMontagePrivate::NotifyClassPath);
	if (!NotifyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraFireBlastAsset] AN_MontageEvent class is unavailable: %s"), AuraCreateFireBlastMontagePrivate::NotifyClassPath);
		return 1;
	}

	UPackage* Package = CreatePackage(AuraCreateFireBlastMontagePrivate::OutputPackageName);
	if (!Package)
	{
		return 1;
	}

	UAnimMontage* Montage = DuplicateObject<UAnimMontage>(SourceMontage, Package, AuraCreateFireBlastMontagePrivate::OutputObjectName);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraFireBlastAsset] Failed to duplicate source montage."));
		return 1;
	}

	Montage->Notifies.Empty();
	const FGameplayTag ReleaseTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.FireBlast"));
	FAnimNotifyEvent ReleaseEvent;
	ReleaseEvent.Notify = AuraCreateFireBlastMontagePrivate::CreateEventNotify(Montage, NotifyClass, ReleaseTag);
	if (!ReleaseEvent.Notify)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraFireBlastAsset] Failed to create FireBlast release notify."));
		return 1;
	}
	ReleaseEvent.NotifyName = TEXT("FireBlastRelease");
	ReleaseEvent.TrackIndex = 0;
	ReleaseEvent.bTriggerOnDedicatedServer = true;
	ReleaseEvent.Link(Montage, SourceMontage->GetPlayLength() * 0.55f, 0);
	Montage->Notifies.Add(MoveTemp(ReleaseEvent));
	Montage->SortNotifies();
	Montage->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Montage);

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		AuraCreateFireBlastMontagePrivate::OutputPackageName,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Montage, *PackageFilename, SaveArgs))
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraFireBlastAsset] Failed to save %s"), *PackageFilename);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[AuraFireBlastAsset] Created %s from %s length=%.3f releaseTime=%.3f."),
		AuraCreateFireBlastMontagePrivate::OutputPackageName,
		AuraCreateFireBlastMontagePrivate::SourcePath,
		Montage->GetPlayLength(),
		SourceMontage->GetPlayLength() * 0.55f);
	return 0;
}
