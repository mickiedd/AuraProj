// Copyright Druid Mechanics

#include "UI/Widget/LoginMenuWidget.h"
#include "Game/LoginPlayerController.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

bool ULoginMenuWidget::InitializeForPlayerController(ALoginPlayerController* InOwnerController)
{
	OwnerLoginPlayerController = InOwnerController;
	AvailableServerTargets.Reset();

	if (!ComboBoxList || !ConnectBtn)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginMenuWidget] WBP_LoginMenu must contain ComboBoxList and ConnectBtn widgets"));
		return false;
	}

	ComboBoxList->OnSelectionChanged.RemoveAll(this);
	ComboBoxList->OnSelectionChanged.AddDynamic(this, &ULoginMenuWidget::HandleLevelSelectionChanged);

	ConnectBtn->OnClicked.RemoveAll(this);
	ConnectBtn->OnClicked.AddDynamic(this, &ULoginMenuWidget::HandleConnectButtonClicked);

	if (!LoadServerTargetsFromLevelConfig())
	{
		if (ALoginPlayerController* OwnerController = OwnerLoginPlayerController.Get())
		{
			OwnerController->ShowLoginMenuStatusMessage(TEXT("No dedicated server levels are configured in LevelConfig.json."));
		}
		return true;
	}

	ComboBoxList->ClearOptions();
	for (const FLoginMenuServerTarget& ServerTarget : AvailableServerTargets)
	{
		ComboBoxList->AddOption(ServerTarget.DisplayName);
	}

	if (!AvailableServerTargets.IsEmpty())
	{
		ComboBoxList->SetSelectedOption(AvailableServerTargets[0].DisplayName);
		if (ALoginPlayerController* OwnerController = OwnerLoginPlayerController.Get())
		{
			OwnerController->HandleLoginMenuSelectionChanged(AvailableServerTargets[0].DisplayName, AvailableServerTargets[0].ServerPort);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginMenuWidget] Manual connect UI initialized with %d configured levels"), AvailableServerTargets.Num());
	return true;
}

void ULoginMenuWidget::NativeDestruct()
{
	if (ComboBoxList)
	{
		ComboBoxList->OnSelectionChanged.RemoveAll(this);
	}

	if (ConnectBtn)
	{
		ConnectBtn->OnClicked.RemoveAll(this);
	}

	AvailableServerTargets.Reset();
	OwnerLoginPlayerController = nullptr;

	Super::NativeDestruct();
}

void ULoginMenuWidget::HandleLevelSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const FLoginMenuServerTarget* SelectedTarget = FindServerTargetByDisplayName(SelectedItem);
	if (!SelectedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginMenuWidget] Selected server target not found: %s"), *SelectedItem);
		return;
	}

	if (ALoginPlayerController* OwnerController = OwnerLoginPlayerController.Get())
	{
		OwnerController->HandleLoginMenuSelectionChanged(SelectedTarget->DisplayName, SelectedTarget->ServerPort);
	}
}

void ULoginMenuWidget::HandleConnectButtonClicked()
{
	FString SelectedDisplayName;
	int32 SelectedPort = 0;

	if (ComboBoxList)
	{
		SelectedDisplayName = ComboBoxList->GetSelectedOption();
	}

	if (!SelectedDisplayName.IsEmpty())
	{
		if (const FLoginMenuServerTarget* SelectedTarget = FindServerTargetByDisplayName(SelectedDisplayName))
		{
			SelectedPort = SelectedTarget->ServerPort;
		}
	}

	if (ALoginPlayerController* OwnerController = OwnerLoginPlayerController.Get())
	{
		OwnerController->RequestLoginMenuConnect(SelectedDisplayName, SelectedPort);
	}
}

bool ULoginMenuWidget::LoadServerTargetsFromLevelConfig()
{
	AvailableServerTargets.Reset();

	const FString ConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LevelConfig.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginMenuWidget] Failed to read level config file: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginMenuWidget] Failed to parse level config JSON: %s"), *ConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("levels"), LevelsArray) || LevelsArray == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginMenuWidget] Level config JSON does not contain a levels array: %s"), *ConfigPath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& LevelValue : *LevelsArray)
	{
		const TSharedPtr<FJsonObject>* LevelObject = nullptr;
		if (!LevelValue.IsValid() || !LevelValue->TryGetObject(LevelObject) || LevelObject == nullptr || !LevelObject->IsValid())
		{
			continue;
		}

		FLoginMenuServerTarget ServerTarget;
		if (!(*LevelObject)->TryGetStringField(TEXT("displayName"), ServerTarget.DisplayName) || ServerTarget.DisplayName.IsEmpty())
		{
			continue;
		}

		(*LevelObject)->TryGetStringField(TEXT("mapPath"), ServerTarget.MapPath);

		double PortValue = 0.0;
		if ((*LevelObject)->TryGetNumberField(TEXT("port"), PortValue))
		{
			ServerTarget.ServerPort = static_cast<int32>(PortValue);
		}

		double QueryPortValue = 0.0;
		if ((*LevelObject)->TryGetNumberField(TEXT("queryPort"), QueryPortValue))
		{
			ServerTarget.QueryPort = static_cast<int32>(QueryPortValue);
		}

		if (ServerTarget.ServerPort >= 1 && ServerTarget.ServerPort <= 65535)
		{
			AvailableServerTargets.Add(ServerTarget);
		}
	}

	return !AvailableServerTargets.IsEmpty();
}

const ULoginMenuWidget::FLoginMenuServerTarget* ULoginMenuWidget::FindServerTargetByDisplayName(const FString& DisplayName) const
{
	for (const FLoginMenuServerTarget& ServerTarget : AvailableServerTargets)
	{
		if (ServerTarget.DisplayName.Equals(DisplayName, ESearchCase::CaseSensitive))
		{
			return &ServerTarget;
		}
	}

	return nullptr;
}
