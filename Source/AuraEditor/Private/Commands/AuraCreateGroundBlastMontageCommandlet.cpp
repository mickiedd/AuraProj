#include "Commands/AuraCreateGroundBlastMontageCommandlet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

int32 UAuraCreateGroundBlastMontageCommandlet::Main(const FString& Params)
{
	UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/Ability_Combo_04V4.Ability_Combo_04V4"));
	if (!Source || !Source->GetSkeleton() || Source->GetPlayLength() <= 0.f) return 1;
	const FString PackageName = TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_GroundBlast_Aura");
	UPackage* Package = CreatePackage(*PackageName);
	UAnimMontage* Montage = NewObject<UAnimMontage>(Package, TEXT("AM_GroundBlast_Aura"), RF_Public | RF_Standalone);
	Montage->SetSkeleton(Source->GetSkeleton());
	Montage->SlotAnimTracks.Reset();
	Montage->CompositeSections.Reset();
	FSlotAnimationTrack Slot;
	Slot.SlotName = TEXT("DefaultSlot");
	FAnimSegment Segment;
	Segment.SetAnimReference(Source);
	Segment.AnimStartTime = 0.f;
	Segment.AnimEndTime = Source->GetPlayLength();
	Segment.AnimPlayRate = 1.f;
	Segment.LoopingCount = 1;
	Slot.AnimTrack.AnimSegments.Add(Segment);
	Montage->SlotAnimTracks.Add(Slot);
	Montage->GetController().SetPlayLength(Montage->CalculateSequenceLength(), false);
	FCompositeSection Section;
	Section.SectionName = TEXT("Default");
	Section.Link(Montage, 0.f);
	Montage->CompositeSections.Add(Section);
	Montage->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Montage);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Montage, *Filename, SaveArgs)) return 1;
	UE_LOG(LogTemp, Display, TEXT("[GroundBlastAsset] Saved %s length=%.3f skeleton=%s"),
		*Filename, Montage->GetPlayLength(), *GetNameSafe(Source->GetSkeleton()));
	return 0;
}
