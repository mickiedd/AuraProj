// Copyright Druid Mechanics

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DesktopPlatformModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Json.h"
#include "LevelEditor.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "PackageTools.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FAuraEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogAuraEditor, Log, All);

class FAuraEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Startup begin | ProjectDir='%s' | ToolMenuUIEnabled=%s | LevelEditorAlreadyLoaded=%s"),
			*FPaths::ProjectDir(),
			UToolMenus::IsToolMenuUIEnabled() ? TEXT("true") : TEXT("false"),
			FModuleManager::Get().IsModuleLoaded("LevelEditor") ? TEXT("true") : TEXT("false"));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		ToolbarExtender = MakeShared<FExtender>();
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender created | IsValid=%s"), ToolbarExtender.IsValid() ? TEXT("true") : TEXT("false"));

		ToolbarExtender->AddToolBarExtension(
			"Play",
			EExtensionHook::After,
			nullptr,
			FToolBarExtensionDelegate::CreateRaw(this, &FAuraEditorModule::AddLegacyToolbarButton));
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender registered on hook='Play' (After)"));

		// Register immediately if ToolMenus is already initialized.
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Immediate ToolMenus registration triggered"));
			RegisterMenus();
		}
		else
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Immediate ToolMenus registration skipped because UI is disabled"));
		}

		// Also register via startup callback to cover first-time menu construction.
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAuraEditorModule::RegisterMenus));
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus startup callback registered"));
		UE_LOG(LogAuraEditor, Display, TEXT("Startup complete"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Shutdown begin"));

		if (ToolbarExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);
			ToolbarExtender.Reset();
			UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender removed"));
		}
		else
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender removal skipped | IsValid=%s | LevelEditorLoaded=%s"),
				ToolbarExtender.IsValid() ? TEXT("true") : TEXT("false"),
				FModuleManager::Get().IsModuleLoaded("LevelEditor") ? TEXT("true") : TEXT("false"));
		}

		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus callbacks/owner unregistered"));
		UE_LOG(LogAuraEditor, Display, TEXT("Shutdown complete"));
	}

private:
	void AddLegacyToolbarButton(FToolBarBuilder& ToolbarBuilder)
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy toolbar builder callback fired; adding combo button"));

		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FAuraEditorModule::GenerateLaunchMenuContent),
			LOCTEXT("LaunchMenuLabel", "Launch"),
			LOCTEXT("LaunchMenuTooltip", "Open launch and build actions for Aura."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));
	}

	TSharedRef<SWidget> GenerateLaunchMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection("AuraLaunchSection", LOCTEXT("AuraLaunchSectionLabel", "Launch"));
		MenuBuilder.AddMenuEntry(
			GetDedicatedServerMenuLabel(),
			GetDedicatedServerMenuTooltip(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnStartDedicatedServerClicked)));

		MenuBuilder.AddMenuEntry(
			GetBuildClientMenuLabel(),
			GetBuildClientMenuTooltip(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.PackageProject"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnBuildClientClicked)));
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("AuraBlueprintToolsSection", LOCTEXT("AuraBlueprintToolsSectionLabel", "Blueprint Snapshot"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportBlueprintSnapshotLabel", "Export Selected Blueprint to JSON"),
			LOCTEXT("ExportBlueprintSnapshotTooltip", "Export the selected Blueprint into an LLM-readable JSON snapshot that also embeds the exact package bytes for full recovery."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnExportSelectedBlueprintToJsonClicked)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportAllBlueprintSnapshotsLabel", "Export All Project Blueprints to JSON"),
			LOCTEXT("ExportAllBlueprintSnapshotsTooltip", "Iterate through all Blueprint assets in this project and export one JSON snapshot next to each Blueprint package file."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnExportAllBlueprintsToJsonClicked)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportAllBehaviorTreeSnapshotsLabel", "Export All Project Behavior Trees to JSON"),
			LOCTEXT("ExportAllBehaviorTreeSnapshotsTooltip", "Iterate through all Behavior Tree assets in this project and export one JSON snapshot next to each Behavior Tree package file."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnExportAllBehaviorTreesToJsonClicked)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportBlueprintSnapshotLabel", "Import Blueprint from JSON Snapshot"),
			LOCTEXT("ImportBlueprintSnapshotTooltip", "Restore a Blueprint package from a previously exported JSON snapshot."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnImportBlueprintFromJsonClicked)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportBehaviorTreeSnapshotLabel", "Import Behavior Tree from JSON Snapshot"),
			LOCTEXT("ImportBehaviorTreeSnapshotTooltip", "Restore a Behavior Tree package from a previously exported JSON snapshot."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnImportBehaviorTreeFromJsonClicked)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportBehaviorTreeSnapshotLabel", "Export Selected Behavior Tree to JSON"),
			LOCTEXT("ExportBehaviorTreeSnapshotTooltip", "Export the selected Behavior Tree into an LLM-readable JSON snapshot that also embeds the exact package bytes for full recovery."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnExportSelectedBehaviorTreeToJsonClicked)));
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	bool TryGetDesktopPlatform(IDesktopPlatform*& OutDesktopPlatform) const
	{
		OutDesktopPlatform = FDesktopPlatformModule::Get();

		if (OutDesktopPlatform != nullptr)
		{
			return true;
		}

		UE_LOG(LogAuraEditor, Error, TEXT("DesktopPlatform module is unavailable"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DesktopPlatformUnavailable", "Desktop platform dialogs are unavailable in this editor session."));
		return false;
	}

	bool TryGetSingleSelectedBlueprint(FAssetData& OutBlueprintAsset, UBlueprint*& OutBlueprint, FText& OutFailureReason) const
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssets;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

		if (SelectedAssets.Num() != 1)
		{
			OutFailureReason = LOCTEXT("BlueprintSelectionCountError", "Select exactly one Blueprint asset in the Content Browser before exporting.");
			return false;
		}

		UBlueprint* SelectedBlueprint = Cast<UBlueprint>(SelectedAssets[0].GetAsset());

		if (SelectedBlueprint == nullptr)
		{
			OutFailureReason = LOCTEXT("BlueprintSelectionTypeError", "The selected asset is not a Blueprint.");
			return false;
		}

		OutBlueprintAsset = SelectedAssets[0];
		OutBlueprint = SelectedBlueprint;
		return true;
	}

	bool TryGetSingleSelectedBehaviorTree(FAssetData& OutBehaviorTreeAsset, UBehaviorTree*& OutBehaviorTree, FText& OutFailureReason) const
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssets;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

		if (SelectedAssets.Num() != 1)
		{
			OutFailureReason = LOCTEXT("BehaviorTreeSelectionCountError", "Select exactly one Behavior Tree asset in the Content Browser before exporting.");
			return false;
		}

		UBehaviorTree* SelectedBehaviorTree = Cast<UBehaviorTree>(SelectedAssets[0].GetAsset());

		if (SelectedBehaviorTree == nullptr)
		{
			OutFailureReason = LOCTEXT("BehaviorTreeSelectionTypeError", "The selected asset is not a Behavior Tree.");
			return false;
		}

		OutBehaviorTreeAsset = SelectedAssets[0];
		OutBehaviorTree = SelectedBehaviorTree;
		return true;
	}

	FString GetPinDirectionString(const UEdGraphPin* Pin) const
	{
		if (Pin == nullptr)
		{
			return TEXT("Unknown");
		}

		if (const UEnum* PinDirectionEnum = StaticEnum<EEdGraphPinDirection>())
		{
			return PinDirectionEnum->GetNameStringByValue(static_cast<int64>(Pin->Direction));
		}

		return TEXT("Unknown");
	}

	FString GetPinContainerTypeString(const FEdGraphPinType& PinType) const
	{
		if (const UEnum* PinContainerEnum = StaticEnum<EPinContainerType>())
		{
			return PinContainerEnum->GetNameStringByValue(static_cast<int64>(PinType.ContainerType));
		}

		return TEXT("None");
	}

	TSharedPtr<FJsonObject> SerializePinType(const FEdGraphPinType& PinType) const
	{
		TSharedPtr<FJsonObject> PinTypeObject = MakeShared<FJsonObject>();
		PinTypeObject->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
		PinTypeObject->SetStringField(TEXT("subcategory"), PinType.PinSubCategory.ToString());
		PinTypeObject->SetStringField(TEXT("subcategoryObjectPath"), GetPathNameSafe(PinType.PinSubCategoryObject.Get()));
		PinTypeObject->SetStringField(TEXT("memberParentClassPath"), GetPathNameSafe(PinType.PinSubCategoryMemberReference.GetMemberParentClass()));
		PinTypeObject->SetStringField(TEXT("memberName"), PinType.PinSubCategoryMemberReference.MemberName.ToString());
		PinTypeObject->SetStringField(TEXT("memberGuid"), PinType.PinSubCategoryMemberReference.MemberGuid.ToString());
		PinTypeObject->SetStringField(TEXT("containerType"), GetPinContainerTypeString(PinType));
		PinTypeObject->SetBoolField(TEXT("isReference"), PinType.bIsReference);
		PinTypeObject->SetBoolField(TEXT("isConst"), PinType.bIsConst);
		PinTypeObject->SetBoolField(TEXT("isWeakPointer"), PinType.bIsWeakPointer);
		PinTypeObject->SetBoolField(TEXT("isUObjectWrapper"), PinType.bIsUObjectWrapper);

		TSharedPtr<FJsonObject> TerminalTypeObject = MakeShared<FJsonObject>();
		TerminalTypeObject->SetStringField(TEXT("category"), PinType.PinValueType.TerminalCategory.ToString());
		TerminalTypeObject->SetStringField(TEXT("subcategory"), PinType.PinValueType.TerminalSubCategory.ToString());
		TerminalTypeObject->SetStringField(TEXT("subcategoryObjectPath"), GetPathNameSafe(PinType.PinValueType.TerminalSubCategoryObject.Get()));
		TerminalTypeObject->SetBoolField(TEXT("isConst"), PinType.PinValueType.bTerminalIsConst);
		TerminalTypeObject->SetBoolField(TEXT("isWeakPointer"), PinType.PinValueType.bTerminalIsWeakPointer);
		TerminalTypeObject->SetBoolField(TEXT("isUObjectWrapper"), PinType.PinValueType.bTerminalIsUObjectWrapper);
		PinTypeObject->SetObjectField(TEXT("terminalType"), TerminalTypeObject);

		return PinTypeObject;
	}

	TSharedPtr<FJsonObject> SerializePin(const UEdGraphPin* Pin) const
	{
		TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
		PinObject->SetStringField(TEXT("name"), Pin != nullptr ? Pin->PinName.ToString() : TEXT(""));
		PinObject->SetStringField(TEXT("pinId"), Pin != nullptr ? Pin->PersistentGuid.ToString() : TEXT(""));
		PinObject->SetStringField(TEXT("direction"), GetPinDirectionString(Pin));
		PinObject->SetStringField(TEXT("defaultValue"), Pin != nullptr ? Pin->DefaultValue : TEXT(""));
		PinObject->SetStringField(TEXT("defaultObjectPath"), Pin != nullptr ? GetPathNameSafe(Pin->DefaultObject) : TEXT(""));
		PinObject->SetStringField(TEXT("defaultTextValue"), Pin != nullptr ? Pin->DefaultTextValue.ToString() : TEXT(""));
		PinObject->SetBoolField(TEXT("isOrphaned"), Pin != nullptr ? Pin->bOrphanedPin : false);
		PinObject->SetBoolField(TEXT("isHidden"), Pin != nullptr ? Pin->bHidden : false);
		PinObject->SetBoolField(TEXT("isNotConnectable"), Pin != nullptr ? Pin->bNotConnectable : false);
		PinObject->SetBoolField(TEXT("isDefaultValueReadOnly"), Pin != nullptr ? Pin->bDefaultValueIsReadOnly : false);
		PinObject->SetBoolField(TEXT("isDefaultValueIgnored"), Pin != nullptr ? Pin->bDefaultValueIsIgnored : false);

		if (Pin != nullptr)
		{
			PinObject->SetObjectField(TEXT("pinType"), SerializePinType(Pin->PinType));

			TArray<TSharedPtr<FJsonValue>> LinkedPins;
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin == nullptr || LinkedPin->GetOwningNodeUnchecked() == nullptr)
				{
					continue;
				}

				TSharedPtr<FJsonObject> LinkedPinObject = MakeShared<FJsonObject>();
				LinkedPinObject->SetStringField(TEXT("nodeGuid"), LinkedPin->GetOwningNodeUnchecked()->NodeGuid.ToString());
				LinkedPinObject->SetStringField(TEXT("pinName"), LinkedPin->PinName.ToString());
				LinkedPinObject->SetStringField(TEXT("pinId"), LinkedPin->PersistentGuid.ToString());
				LinkedPins.Add(MakeShared<FJsonValueObject>(LinkedPinObject));
			}

			PinObject->SetArrayField(TEXT("linkedTo"), LinkedPins);
		}

		return PinObject;
	}

	TSharedPtr<FJsonObject> SerializeNode(const UEdGraphNode* Node) const
	{
		TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("guid"), Node != nullptr ? Node->NodeGuid.ToString() : TEXT(""));
		NodeObject->SetStringField(TEXT("name"), Node != nullptr ? Node->GetName() : TEXT(""));
		NodeObject->SetStringField(TEXT("classPath"), Node != nullptr ? Node->GetClass()->GetPathName() : TEXT(""));
		NodeObject->SetStringField(TEXT("title"), Node != nullptr ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT(""));
		NodeObject->SetStringField(TEXT("comment"), Node != nullptr ? Node->NodeComment : TEXT(""));
		NodeObject->SetNumberField(TEXT("positionX"), Node != nullptr ? Node->NodePosX : 0);
		NodeObject->SetNumberField(TEXT("positionY"), Node != nullptr ? Node->NodePosY : 0);
		NodeObject->SetBoolField(TEXT("enabled"), Node != nullptr ? Node->IsNodeEnabled() : false);
		NodeObject->SetStringField(TEXT("errorMessage"), Node != nullptr ? Node->ErrorMsg : TEXT(""));

		TArray<TSharedPtr<FJsonValue>> PinArray;
		if (Node != nullptr)
		{
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				PinArray.Add(MakeShared<FJsonValueObject>(SerializePin(Pin)));
			}
		}

		NodeObject->SetArrayField(TEXT("pins"), PinArray);
		return NodeObject;
	}

	TSharedPtr<FJsonObject> SerializeGraph(const UEdGraph* Graph) const
	{
		TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
		GraphObject->SetStringField(TEXT("name"), Graph != nullptr ? Graph->GetName() : TEXT(""));
		GraphObject->SetStringField(TEXT("displayName"), Graph != nullptr ? Graph->GetFName().ToString() : TEXT(""));
		GraphObject->SetStringField(TEXT("classPath"), Graph != nullptr ? Graph->GetClass()->GetPathName() : TEXT(""));
		GraphObject->SetStringField(TEXT("schemaPath"), Graph != nullptr && Graph->GetSchema() != nullptr ? Graph->GetSchema()->GetPathName() : TEXT(""));

		TArray<TSharedPtr<FJsonValue>> NodeArray;
		if (Graph != nullptr)
		{
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				NodeArray.Add(MakeShared<FJsonValueObject>(SerializeNode(Node)));
			}
		}

		GraphObject->SetArrayField(TEXT("nodes"), NodeArray);
		return GraphObject;
	}

	void AddGraphsToArray(const TArray<UEdGraph*>& Graphs, TArray<TSharedPtr<FJsonValue>>& OutGraphs) const
	{
		for (const UEdGraph* Graph : Graphs)
		{
			OutGraphs.Add(MakeShared<FJsonValueObject>(SerializeGraph(Graph)));
		}
	}

	TSharedPtr<FJsonObject> SerializeBlueprintForAnalysis(const UBlueprint* Blueprint) const
	{
		TSharedPtr<FJsonObject> BlueprintObject = MakeShared<FJsonObject>();
		BlueprintObject->SetStringField(TEXT("assetName"), Blueprint != nullptr ? Blueprint->GetName() : TEXT(""));
		BlueprintObject->SetStringField(TEXT("classPath"), Blueprint != nullptr ? Blueprint->GetClass()->GetPathName() : TEXT(""));
		BlueprintObject->SetStringField(TEXT("parentClassPath"), Blueprint != nullptr ? GetPathNameSafe(Blueprint->ParentClass) : TEXT(""));
		BlueprintObject->SetStringField(TEXT("generatedClassPath"), Blueprint != nullptr ? GetPathNameSafe(Blueprint->GeneratedClass) : TEXT(""));
		BlueprintObject->SetStringField(TEXT("skeletonClassPath"), Blueprint != nullptr ? GetPathNameSafe(Blueprint->SkeletonGeneratedClass) : TEXT(""));

		TArray<TSharedPtr<FJsonValue>> Graphs;
		if (Blueprint != nullptr)
		{
			AddGraphsToArray(Blueprint->UbergraphPages, Graphs);
			AddGraphsToArray(Blueprint->FunctionGraphs, Graphs);
			AddGraphsToArray(Blueprint->MacroGraphs, Graphs);
			AddGraphsToArray(Blueprint->DelegateSignatureGraphs, Graphs);
		}

		BlueprintObject->SetArrayField(TEXT("graphs"), Graphs);
		return BlueprintObject;
	}

	TSharedPtr<FJsonObject> SerializeBehaviorTreeDecorator(const UBTDecorator* Decorator) const
	{
		TSharedPtr<FJsonObject> DecoratorObject = MakeShared<FJsonObject>();
		DecoratorObject->SetStringField(TEXT("name"), Decorator != nullptr ? Decorator->GetName() : TEXT(""));
		DecoratorObject->SetStringField(TEXT("classPath"), Decorator != nullptr ? Decorator->GetClass()->GetPathName() : TEXT(""));
		DecoratorObject->SetStringField(TEXT("nodeName"), Decorator != nullptr ? Decorator->GetNodeName() : TEXT(""));
		DecoratorObject->SetStringField(TEXT("path"), GetPathNameSafe(Decorator));
		return DecoratorObject;
	}

	TSharedPtr<FJsonObject> SerializeBehaviorTreeService(const UBTService* Service) const
	{
		TSharedPtr<FJsonObject> ServiceObject = MakeShared<FJsonObject>();
		ServiceObject->SetStringField(TEXT("name"), Service != nullptr ? Service->GetName() : TEXT(""));
		ServiceObject->SetStringField(TEXT("classPath"), Service != nullptr ? Service->GetClass()->GetPathName() : TEXT(""));
		ServiceObject->SetStringField(TEXT("nodeName"), Service != nullptr ? Service->GetNodeName() : TEXT(""));
		ServiceObject->SetStringField(TEXT("path"), GetPathNameSafe(Service));
		return ServiceObject;
	}

	template <typename TDecoratorArrayType>
	TArray<TSharedPtr<FJsonValue>> SerializeBehaviorTreeDecorators(const TDecoratorArrayType& Decorators) const
	{
		TArray<TSharedPtr<FJsonValue>> DecoratorArray;
		for (const UBTDecorator* Decorator : Decorators)
		{
			DecoratorArray.Add(MakeShared<FJsonValueObject>(SerializeBehaviorTreeDecorator(Decorator)));
		}

		return DecoratorArray;
	}

	template <typename TServiceArrayType>
	TArray<TSharedPtr<FJsonValue>> SerializeBehaviorTreeServices(const TServiceArrayType& Services) const
	{
		TArray<TSharedPtr<FJsonValue>> ServiceArray;
		for (const UBTService* Service : Services)
		{
			ServiceArray.Add(MakeShared<FJsonValueObject>(SerializeBehaviorTreeService(Service)));
		}

		return ServiceArray;
	}

	TSharedPtr<FJsonObject> SerializeBehaviorTreeNodeRecursive(const UBTNode* Node, TSet<const UBTNode*>& InOutVisitedNodes) const
	{
		TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();

		if (Node == nullptr)
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("None"));
			return NodeObject;
		}

		NodeObject->SetStringField(TEXT("name"), Node->GetName());
		NodeObject->SetStringField(TEXT("classPath"), Node->GetClass()->GetPathName());
		NodeObject->SetStringField(TEXT("nodeName"), Node->GetNodeName());
		NodeObject->SetStringField(TEXT("staticDescription"), Node->GetStaticDescription());
		NodeObject->SetStringField(TEXT("path"), GetPathNameSafe(Node));

		if (InOutVisitedNodes.Contains(Node))
		{
			NodeObject->SetBoolField(TEXT("isReferenceOnly"), true);
			return NodeObject;
		}

		InOutVisitedNodes.Add(Node);
		NodeObject->SetBoolField(TEXT("isReferenceOnly"), false);

		if (const UBTCompositeNode* CompositeNode = Cast<UBTCompositeNode>(Node))
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("Composite"));
			NodeObject->SetArrayField(TEXT("services"), SerializeBehaviorTreeServices(CompositeNode->Services));

			TArray<TSharedPtr<FJsonValue>> Children;
			for (const FBTCompositeChild& Child : CompositeNode->Children)
			{
				TSharedPtr<FJsonObject> ChildObject = MakeShared<FJsonObject>();
				ChildObject->SetArrayField(TEXT("decorators"), SerializeBehaviorTreeDecorators(Child.Decorators));

				TArray<TSharedPtr<FJsonValue>> DecoratorOps;
				for (const FBTDecoratorLogic& DecoratorOp : Child.DecoratorOps)
				{
					TSharedPtr<FJsonObject> DecoratorOpObject = MakeShared<FJsonObject>();
					DecoratorOpObject->SetNumberField(TEXT("operation"), static_cast<int32>(DecoratorOp.Operation));
					DecoratorOpObject->SetNumberField(TEXT("number"), DecoratorOp.Number);
					DecoratorOps.Add(MakeShared<FJsonValueObject>(DecoratorOpObject));
				}

				ChildObject->SetArrayField(TEXT("decoratorOps"), DecoratorOps);

				if (Child.ChildComposite != nullptr)
				{
					ChildObject->SetStringField(TEXT("childType"), TEXT("Composite"));
					ChildObject->SetObjectField(TEXT("childNode"), SerializeBehaviorTreeNodeRecursive(Child.ChildComposite, InOutVisitedNodes));
				}
				else if (Child.ChildTask != nullptr)
				{
					ChildObject->SetStringField(TEXT("childType"), TEXT("Task"));
					ChildObject->SetObjectField(TEXT("childNode"), SerializeBehaviorTreeNodeRecursive(Child.ChildTask, InOutVisitedNodes));
				}
				else
				{
					ChildObject->SetStringField(TEXT("childType"), TEXT("None"));
				}

				Children.Add(MakeShared<FJsonValueObject>(ChildObject));
			}

			NodeObject->SetArrayField(TEXT("children"), Children);
		}
		else if (const UBTTaskNode* TaskNode = Cast<UBTTaskNode>(Node))
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("Task"));
			NodeObject->SetStringField(TEXT("taskClassPath"), TaskNode->GetClass()->GetPathName());
		}
		else if (Cast<UBTDecorator>(Node) != nullptr)
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("Decorator"));
		}
		else if (Cast<UBTService>(Node) != nullptr)
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("Service"));
		}
		else
		{
			NodeObject->SetStringField(TEXT("nodeType"), TEXT("Unknown"));
		}

		return NodeObject;
	}

	TSharedPtr<FJsonObject> SerializeBehaviorTreeForAnalysis(const UBehaviorTree* BehaviorTree) const
	{
		TSharedPtr<FJsonObject> BehaviorTreeObject = MakeShared<FJsonObject>();
		BehaviorTreeObject->SetStringField(TEXT("assetName"), BehaviorTree != nullptr ? BehaviorTree->GetName() : TEXT(""));
		BehaviorTreeObject->SetStringField(TEXT("classPath"), BehaviorTree != nullptr ? BehaviorTree->GetClass()->GetPathName() : TEXT(""));
		BehaviorTreeObject->SetStringField(TEXT("path"), GetPathNameSafe(BehaviorTree));
		BehaviorTreeObject->SetStringField(TEXT("blackboardAssetPath"), BehaviorTree != nullptr ? GetPathNameSafe(BehaviorTree->BlackboardAsset.Get()) : TEXT(""));

		if (BehaviorTree != nullptr)
		{
			TSet<const UBTNode*> VisitedNodes;
			BehaviorTreeObject->SetObjectField(TEXT("rootNode"), SerializeBehaviorTreeNodeRecursive(BehaviorTree->RootNode, VisitedNodes));
			BehaviorTreeObject->SetNumberField(TEXT("uniqueNodeCount"), VisitedNodes.Num());
		}
		else
		{
			BehaviorTreeObject->SetObjectField(TEXT("rootNode"), MakeShared<FJsonObject>());
			BehaviorTreeObject->SetNumberField(TEXT("uniqueNodeCount"), 0);
		}

		return BehaviorTreeObject;
	}

	bool BuildPackageSnapshot(const FString& PackageName, TArray<TSharedPtr<FJsonValue>>& OutPackageFiles, FText& OutFailureReason) const
	{
		const FString PackageBaseFilename = FPackageName::LongPackageNameToFilename(PackageName);
		const FString PackageDirectory = FPaths::GetPath(PackageBaseFilename);
		const FString PackageFileStem = FPaths::GetBaseFilename(PackageBaseFilename);
		TArray<FString> PackageFilenames;
		IFileManager::Get().FindFiles(PackageFilenames, *(PackageDirectory / (PackageFileStem + TEXT(".*"))), true, false);

		if (PackageFilenames.IsEmpty())
		{
			OutFailureReason = FText::Format(
				LOCTEXT("BlueprintSnapshotNoPackageFiles", "Could not locate package files for {0}."),
				FText::FromString(PackageName));
			return false;
		}

		for (const FString& PackageFilename : PackageFilenames)
		{
			const FString FullPackagePath = PackageDirectory / PackageFilename;
			TArray<uint8> FileBytes;

			if (!FFileHelper::LoadFileToArray(FileBytes, *FullPackagePath))
			{
				OutFailureReason = FText::Format(
					LOCTEXT("BlueprintSnapshotReadFailure", "Failed to read package file {0}."),
					FText::FromString(FullPackagePath));
				return false;
			}

			FString RelativePackagePath = FullPackagePath;
			FPaths::MakePathRelativeTo(RelativePackagePath, *FPaths::ProjectDir());

			TSharedPtr<FJsonObject> PackageFileObject = MakeShared<FJsonObject>();
			PackageFileObject->SetStringField(TEXT("projectRelativePath"), RelativePackagePath);
			PackageFileObject->SetStringField(TEXT("fileName"), PackageFilename);
			PackageFileObject->SetNumberField(TEXT("byteCount"), FileBytes.Num());
			PackageFileObject->SetStringField(TEXT("contentBase64"), FBase64::Encode(FileBytes));
			OutPackageFiles.Add(MakeShared<FJsonValueObject>(PackageFileObject));
		}

		return true;
	}

	bool PromptForSaveFile(const FString& DefaultFilename, FString& OutFilename) const
	{
		return PromptForSaveFileWithSettings(
			DefaultFilename,
			TEXT("Export Blueprint JSON Snapshot"),
			TEXT("Saved/BlueprintSnapshots"),
			OutFilename);
	}

	bool PromptForSaveFileWithSettings(const FString& DefaultFilename, const FString& DialogTitle, const FString& RelativeOutputDirectory, FString& OutFilename) const
	{
		IDesktopPlatform* DesktopPlatform = nullptr;
		if (!TryGetDesktopPlatform(DesktopPlatform))
		{
			return false;
		}

		const FString ExportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeOutputDirectory);
		IFileManager::Get().MakeDirectory(*ExportDirectory, true);

		TArray<FString> SaveFilenames;
		const bool bPickedFile = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			ExportDirectory,
			DefaultFilename,
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			SaveFilenames);

		if (!bPickedFile || SaveFilenames.IsEmpty())
		{
			return false;
		}

		OutFilename = SaveFilenames[0];
		return true;
	}

	bool PromptForOpenFile(FString& OutFilename) const
	{
		return PromptForOpenFileWithSettings(
			TEXT("Import Blueprint JSON Snapshot"),
			TEXT("Saved/BlueprintSnapshots"),
			OutFilename);
	}

	bool PromptForOpenFileWithSettings(const FString& DialogTitle, const FString& RelativeInputDirectory, FString& OutFilename) const
	{
		IDesktopPlatform* DesktopPlatform = nullptr;
		if (!TryGetDesktopPlatform(DesktopPlatform))
		{
			return false;
		}

		const FString SnapshotDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeInputDirectory);
		TArray<FString> OpenFilenames;
		const bool bPickedFile = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			SnapshotDirectory,
			TEXT(""),
			TEXT("JSON Files (*.json)|*.json"),
			EFileDialogFlags::None,
			OpenFilenames);

		if (!bPickedFile || OpenFilenames.IsEmpty())
		{
			return false;
		}

		OutFilename = OpenFilenames[0];
		return true;
	}

	bool BuildBlueprintSnapshotJson(const FAssetData& BlueprintAsset, const UBlueprint* Blueprint, FString& OutJson, FText& OutFailureReason) const
	{
		TArray<TSharedPtr<FJsonValue>> PackageFiles;
		if (!BuildPackageSnapshot(BlueprintAsset.PackageName.ToString(), PackageFiles, OutFailureReason))
		{
			return false;
		}

		TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("snapshotType"), TEXT("AuraBlueprintSnapshot"));
		RootObject->SetStringField(TEXT("schemaVersion"), TEXT("1"));
		RootObject->SetStringField(TEXT("assetName"), BlueprintAsset.AssetName.ToString());
		RootObject->SetStringField(TEXT("packageName"), BlueprintAsset.PackageName.ToString());
		RootObject->SetStringField(TEXT("objectPath"), BlueprintAsset.GetObjectPathString());
		RootObject->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		RootObject->SetObjectField(TEXT("analysis"), SerializeBlueprintForAnalysis(Blueprint));
		RootObject->SetArrayField(TEXT("packageFiles"), PackageFiles);

		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			OutFailureReason = LOCTEXT("BlueprintSnapshotSerializeFailure", "Failed to serialize Blueprint snapshot JSON.");
			return false;
		}

		return true;
	}

	bool SaveBlueprintSnapshotJson(const FAssetData& BlueprintAsset, const UBlueprint* Blueprint, const FString& OutputFilename, FText& OutFailureReason) const
	{
		FString JsonOutput;
		if (!BuildBlueprintSnapshotJson(BlueprintAsset, Blueprint, JsonOutput, OutFailureReason))
		{
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JsonOutput, *OutputFilename))
		{
			OutFailureReason = FText::Format(
				LOCTEXT("BlueprintSnapshotWriteFailureWithPath", "Failed to write Blueprint snapshot JSON file:\n{0}"),
				FText::FromString(OutputFilename));
			return false;
		}

		return true;
	}

	bool BuildBehaviorTreeSnapshotJson(const FAssetData& BehaviorTreeAsset, const UBehaviorTree* BehaviorTree, FString& OutJson, FText& OutFailureReason) const
	{
		TArray<TSharedPtr<FJsonValue>> PackageFiles;
		if (!BuildPackageSnapshot(BehaviorTreeAsset.PackageName.ToString(), PackageFiles, OutFailureReason))
		{
			return false;
		}

		TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("snapshotType"), TEXT("AuraBehaviorTreeSnapshot"));
		RootObject->SetStringField(TEXT("schemaVersion"), TEXT("1"));
		RootObject->SetStringField(TEXT("assetName"), BehaviorTreeAsset.AssetName.ToString());
		RootObject->SetStringField(TEXT("packageName"), BehaviorTreeAsset.PackageName.ToString());
		RootObject->SetStringField(TEXT("objectPath"), BehaviorTreeAsset.GetObjectPathString());
		RootObject->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		RootObject->SetObjectField(TEXT("analysis"), SerializeBehaviorTreeForAnalysis(BehaviorTree));
		RootObject->SetArrayField(TEXT("packageFiles"), PackageFiles);

		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			OutFailureReason = LOCTEXT("BehaviorTreeSnapshotSerializeFailure", "Failed to serialize Behavior Tree snapshot JSON.");
			return false;
		}

		return true;
	}

	bool SaveBehaviorTreeSnapshotJson(const FAssetData& BehaviorTreeAsset, const UBehaviorTree* BehaviorTree, const FString& OutputFilename, FText& OutFailureReason) const
	{
		FString JsonOutput;
		if (!BuildBehaviorTreeSnapshotJson(BehaviorTreeAsset, BehaviorTree, JsonOutput, OutFailureReason))
		{
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JsonOutput, *OutputFilename))
		{
			OutFailureReason = FText::Format(
				LOCTEXT("BehaviorTreeSnapshotWriteFailureWithPath", "Failed to write Behavior Tree snapshot JSON file:\n{0}"),
				FText::FromString(OutputFilename));
			return false;
		}

		return true;
	}

	void OnExportAllBlueprintsToJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Export all project blueprints to JSON clicked"));

		if (FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ConfirmExportAllBlueprintSnapshots", "Export JSON snapshots for all Blueprint assets in this project?\n\nA progress dialog with Cancel will be shown.")) != EAppReturnType::Yes)
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Export all project blueprints canceled at confirmation prompt"));
			return;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> CandidateBlueprintAssets;
		AssetRegistryModule.Get().GetAssets(Filter, CandidateBlueprintAssets);

		TArray<FAssetData> ProjectBlueprintAssets;
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		for (const FAssetData& AssetData : CandidateBlueprintAssets)
		{
			const FString PackageFilename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), TEXT(".uasset")));

			if (PackageFilename.StartsWith(ProjectDir))
			{
				ProjectBlueprintAssets.Add(AssetData);
			}
		}

		if (ProjectBlueprintAssets.IsEmpty())
		{
			UE_LOG(LogAuraEditor, Warning, TEXT("No project blueprint assets found for bulk export"));
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoProjectBlueprintsFound", "No Blueprint assets were found under this project directory."));
			return;
		}

		ProjectBlueprintAssets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.GetObjectPathString() < B.GetObjectPathString();
		});

		FScopedSlowTask SlowTask(
			static_cast<float>(ProjectBlueprintAssets.Num()),
			LOCTEXT("ExportAllBlueprintSnapshotsProgress", "Exporting Blueprint snapshots to JSON..."));
		SlowTask.MakeDialog(true);

		int32 SucceededCount = 0;
		int32 FailedCount = 0;
		int32 CanceledCount = 0;

		for (const FAssetData& AssetData : ProjectBlueprintAssets)
		{
			if (SlowTask.ShouldCancel())
			{
				CanceledCount = ProjectBlueprintAssets.Num() - SucceededCount - FailedCount;
				UE_LOG(LogAuraEditor, Warning, TEXT("Bulk blueprint snapshot export canceled by user | Succeeded=%d | Failed=%d | Remaining=%d"),
					SucceededCount,
					FailedCount,
					CanceledCount);
				break;
			}

			SlowTask.EnterProgressFrame(
				1.0f,
				FText::Format(
					LOCTEXT("ExportBlueprintSnapshotProgressItem", "Exporting {0}"),
					FText::FromName(AssetData.AssetName)));

			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
			if (Blueprint == nullptr)
			{
				++FailedCount;
				UE_LOG(LogAuraEditor, Error, TEXT("Bulk export failed: asset is not a loaded blueprint | ObjectPath='%s'"), *AssetData.GetObjectPathString());
				continue;
			}

			const FString OutputFilename = FPaths::ConvertRelativePathToFull(
				FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), TEXT(".snapshot.json")));

			FText FailureReason;
			if (!SaveBlueprintSnapshotJson(AssetData, Blueprint, OutputFilename, FailureReason))
			{
				++FailedCount;
				UE_LOG(LogAuraEditor, Error, TEXT("Bulk export failed | Asset='%s' | Reason='%s'"), *AssetData.GetObjectPathString(), *FailureReason.ToString());
				continue;
			}

			++SucceededCount;
		}

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("ExportAllBlueprintSnapshotsSummary", "Blueprint snapshot export complete.\n\nSucceeded: {0}\nFailed: {1}\nCanceled/Remaining: {2}\n\nEach JSON was saved next to its Blueprint package file."),
				SucceededCount,
				FailedCount,
				CanceledCount));
	}

	void OnExportAllBehaviorTreesToJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Export all project behavior trees to JSON clicked"));

		if (FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ConfirmExportAllBehaviorTreeSnapshots", "Export JSON snapshots for all Behavior Tree assets in this project?\n\nA progress dialog with Cancel will be shown.")) != EAppReturnType::Yes)
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Export all project behavior trees canceled at confirmation prompt"));
			return;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FARFilter Filter;
		Filter.ClassPaths.Add(UBehaviorTree::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> CandidateBehaviorTreeAssets;
		AssetRegistryModule.Get().GetAssets(Filter, CandidateBehaviorTreeAssets);

		TArray<FAssetData> ProjectBehaviorTreeAssets;
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		for (const FAssetData& AssetData : CandidateBehaviorTreeAssets)
		{
			const FString PackageFilename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), TEXT(".uasset")));

			if (PackageFilename.StartsWith(ProjectDir))
			{
				ProjectBehaviorTreeAssets.Add(AssetData);
			}
		}

		if (ProjectBehaviorTreeAssets.IsEmpty())
		{
			UE_LOG(LogAuraEditor, Warning, TEXT("No project behavior tree assets found for bulk export"));
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoProjectBehaviorTreesFound", "No Behavior Tree assets were found under this project directory."));
			return;
		}

		ProjectBehaviorTreeAssets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.GetObjectPathString() < B.GetObjectPathString();
		});

		FScopedSlowTask SlowTask(
			static_cast<float>(ProjectBehaviorTreeAssets.Num()),
			LOCTEXT("ExportAllBehaviorTreeSnapshotsProgress", "Exporting Behavior Tree snapshots to JSON..."));
		SlowTask.MakeDialog(true);

		int32 SucceededCount = 0;
		int32 FailedCount = 0;
		int32 CanceledCount = 0;

		for (const FAssetData& AssetData : ProjectBehaviorTreeAssets)
		{
			if (SlowTask.ShouldCancel())
			{
				CanceledCount = ProjectBehaviorTreeAssets.Num() - SucceededCount - FailedCount;
				UE_LOG(LogAuraEditor, Warning, TEXT("Bulk behavior tree snapshot export canceled by user | Succeeded=%d | Failed=%d | Remaining=%d"),
					SucceededCount,
					FailedCount,
					CanceledCount);
				break;
			}

			SlowTask.EnterProgressFrame(
				1.0f,
				FText::Format(
					LOCTEXT("ExportBehaviorTreeSnapshotProgressItem", "Exporting {0}"),
					FText::FromName(AssetData.AssetName)));

			UBehaviorTree* BehaviorTree = Cast<UBehaviorTree>(AssetData.GetAsset());
			if (BehaviorTree == nullptr)
			{
				++FailedCount;
				UE_LOG(LogAuraEditor, Error, TEXT("Bulk export failed: asset is not a loaded behavior tree | ObjectPath='%s'"), *AssetData.GetObjectPathString());
				continue;
			}

			const FString OutputFilename = FPaths::ConvertRelativePathToFull(
				FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), TEXT(".snapshot.json")));

			FText FailureReason;
			if (!SaveBehaviorTreeSnapshotJson(AssetData, BehaviorTree, OutputFilename, FailureReason))
			{
				++FailedCount;
				UE_LOG(LogAuraEditor, Error, TEXT("Bulk behavior tree export failed | Asset='%s' | Reason='%s'"), *AssetData.GetObjectPathString(), *FailureReason.ToString());
				continue;
			}

			++SucceededCount;
		}

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("ExportAllBehaviorTreeSnapshotsSummary", "Behavior Tree snapshot export complete.\n\nSucceeded: {0}\nFailed: {1}\nCanceled/Remaining: {2}\n\nEach JSON was saved next to its Behavior Tree package file."),
				SucceededCount,
				FailedCount,
				CanceledCount));
	}

	void OnExportSelectedBlueprintToJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Export selected blueprint to JSON clicked"));

		FAssetData BlueprintAsset;
		UBlueprint* Blueprint = nullptr;
		FText FailureReason;

		if (!TryGetSingleSelectedBlueprint(BlueprintAsset, Blueprint, FailureReason))
		{
			UE_LOG(LogAuraEditor, Warning, TEXT("Blueprint export aborted: %s"), *FailureReason.ToString());
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
			return;
		}

		FString OutputFilename;
		if (!PromptForSaveFile(BlueprintAsset.AssetName.ToString() + TEXT(".snapshot.json"), OutputFilename))
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Blueprint export canceled by user"));
			return;
		}

		if (!SaveBlueprintSnapshotJson(BlueprintAsset, Blueprint, OutputFilename, FailureReason))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Blueprint export failed | Asset='%s' | Reason='%s'"), *BlueprintAsset.GetObjectPathString(), *FailureReason.ToString());
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
			return;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("Blueprint JSON snapshot exported successfully | Asset='%s' | File='%s'"), *BlueprintAsset.AssetName.ToString(), *OutputFilename);
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("BlueprintSnapshotExportSuccess", "Exported Blueprint snapshot to:\n{0}\n\nThe JSON contains both a readable graph summary for LLM analysis and the exact package bytes needed for recovery."),
				FText::FromString(OutputFilename)));
	}

	void OnExportSelectedBehaviorTreeToJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Export selected behavior tree to JSON clicked"));

		FAssetData BehaviorTreeAsset;
		UBehaviorTree* BehaviorTree = nullptr;
		FText FailureReason;

		if (!TryGetSingleSelectedBehaviorTree(BehaviorTreeAsset, BehaviorTree, FailureReason))
		{
			UE_LOG(LogAuraEditor, Warning, TEXT("Behavior Tree export aborted: %s"), *FailureReason.ToString());
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
			return;
		}

		FString OutputFilename;
		if (!PromptForSaveFileWithSettings(
			BehaviorTreeAsset.AssetName.ToString() + TEXT(".snapshot.json"),
			TEXT("Export Behavior Tree JSON Snapshot"),
			TEXT("Saved/BehaviorTreeSnapshots"),
			OutputFilename))
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Behavior Tree export canceled by user"));
			return;
		}

		if (!SaveBehaviorTreeSnapshotJson(BehaviorTreeAsset, BehaviorTree, OutputFilename, FailureReason))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree export failed | Asset='%s' | Reason='%s'"), *BehaviorTreeAsset.GetObjectPathString(), *FailureReason.ToString());
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
			return;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("Behavior Tree JSON snapshot exported successfully | Asset='%s' | File='%s'"), *BehaviorTreeAsset.AssetName.ToString(), *OutputFilename);
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("BehaviorTreeSnapshotExportSuccess", "Exported Behavior Tree snapshot to:\n{0}\n\nThe JSON contains a readable behavior tree summary for LLM analysis and the exact package bytes needed for recovery."),
				FText::FromString(OutputFilename)));
	}

	void OnImportBlueprintFromJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Import blueprint from JSON snapshot clicked"));

		FString InputFilename;
		if (!PromptForOpenFile(InputFilename))
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Blueprint import canceled by user"));
			return;
		}

		FString JsonInput;
		if (!FFileHelper::LoadFileToString(JsonInput, *InputFilename))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: could not read file | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotReadJsonFailure", "Failed to read the selected Blueprint snapshot JSON file."));
			return;
		}

		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonInput);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: invalid JSON | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotInvalidJson", "The selected file is not a valid Blueprint snapshot JSON file."));
			return;
		}

		FString SnapshotType;
		if (!RootObject->TryGetStringField(TEXT("snapshotType"), SnapshotType) || SnapshotType != TEXT("AuraBlueprintSnapshot"))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: unsupported snapshot type | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotUnsupportedType", "The selected JSON file is not an Aura Blueprint snapshot export."));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* PackageFiles = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("packageFiles"), PackageFiles) || PackageFiles == nullptr || PackageFiles->IsEmpty())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: packageFiles array is missing | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotMissingPackageFiles", "The snapshot JSON does not contain any embedded package files."));
			return;
		}

		TArray<UPackage*> LoadedPackagesToReload;

		for (const TSharedPtr<FJsonValue>& PackageFileValue : *PackageFiles)
		{
			const TSharedPtr<FJsonObject>* PackageFileObject = nullptr;
			if (!PackageFileValue.IsValid() || !PackageFileValue->TryGetObject(PackageFileObject) || PackageFileObject == nullptr || !PackageFileObject->IsValid())
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: malformed package file entry | File='%s'"), *InputFilename);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotMalformedPackageEntry", "A package file entry in the snapshot JSON is malformed."));
				return;
			}

			FString RelativePath;
			FString ContentBase64;
			if (!(*PackageFileObject)->TryGetStringField(TEXT("projectRelativePath"), RelativePath) || !(*PackageFileObject)->TryGetStringField(TEXT("contentBase64"), ContentBase64))
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: incomplete package file entry | File='%s'"), *InputFilename);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotIncompletePackageEntry", "A package file entry in the snapshot JSON is incomplete."));
				return;
			}

			TArray<uint8> FileBytes;
			if (!FBase64::Decode(ContentBase64, FileBytes))
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: base64 decode error | File='%s' | Target='%s'"), *InputFilename, *RelativePath);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BlueprintSnapshotDecodeFailure", "Failed to decode embedded package data from the snapshot JSON."));
				return;
			}

			const FString TargetPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath);
			IFileManager& FileManager = IFileManager::Get();
			FileManager.MakeDirectory(*FPaths::GetPath(TargetPath), true);

			if (FileManager.FileExists(*TargetPath) && FileManager.IsReadOnly(*TargetPath))
			{
				const bool bClearedReadOnly = FileManager.Delete(*TargetPath, false, true, true);
				UE_LOG(LogAuraEditor, Display, TEXT("Target package was read-only; attempted delete before restore | Target='%s' | Deleted=%s"),
					*TargetPath,
					bClearedReadOnly ? TEXT("true") : TEXT("false"));
			}

			if (!FFileHelper::SaveArrayToFile(FileBytes, *TargetPath, &FileManager, FILEWRITE_EvenIfReadOnly))
			{
				const bool bTargetExists = FileManager.FileExists(*TargetPath);
				const bool bTargetReadOnly = bTargetExists && FileManager.IsReadOnly(*TargetPath);

				UE_LOG(LogAuraEditor, Error, TEXT("Blueprint import failed: could not write package file | Target='%s' | Exists=%s | ReadOnly=%s"),
					*TargetPath,
					bTargetExists ? TEXT("true") : TEXT("false"),
					bTargetReadOnly ? TEXT("true") : TEXT("false"));

				FMessageDialog::Open(
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("BlueprintSnapshotWritePackageFailure", "Failed to write restored package file:\n{0}\n\nIf this asset is source-controlled, check it out or make it writable, then retry. If the asset is open in another process, close that process and retry."),
						FText::FromString(TargetPath)));
				return;
			}

			FString LongPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(TargetPath, LongPackageName))
			{
				if (UPackage* LoadedPackage = FindPackage(nullptr, *LongPackageName))
				{
					LoadedPackagesToReload.AddUnique(LoadedPackage);
				}
			}
		}

		if (!LoadedPackagesToReload.IsEmpty())
		{
			FText ReloadErrorMessage;
			const bool bReloadedAnyPackages = UPackageTools::ReloadPackages(
				LoadedPackagesToReload,
				ReloadErrorMessage,
				EReloadPackagesInteractionMode::AssumePositive);

			if (!bReloadedAnyPackages)
			{
				UE_LOG(LogAuraEditor, Warning, TEXT("Blueprint snapshot imported, but package reload did not complete | Error='%s'"), *ReloadErrorMessage.ToString());
				FMessageDialog::Open(
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("BlueprintSnapshotImportReloadWarning", "Blueprint package files were restored from JSON, but reload did not complete:\n{0}\n\nUse Asset Actions -> Reload on the restored asset, or restart the editor."),
						ReloadErrorMessage));
				return;
			}
		}

		UE_LOG(LogAuraEditor, Display, TEXT("Blueprint snapshot imported successfully | File='%s'"), *InputFilename);
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("BlueprintSnapshotImportSuccess", "Blueprint package files were restored from the JSON snapshot and any loaded packages were reloaded from disk."));
	}

	void OnImportBehaviorTreeFromJsonClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Import behavior tree from JSON snapshot clicked"));

		FString InputFilename;
		if (!PromptForOpenFileWithSettings(
			TEXT("Import Behavior Tree JSON Snapshot"),
			TEXT("Saved/BehaviorTreeSnapshots"),
			InputFilename))
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Behavior Tree import canceled by user"));
			return;
		}

		FString JsonInput;
		if (!FFileHelper::LoadFileToString(JsonInput, *InputFilename))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: could not read file | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotReadJsonFailure", "Failed to read the selected Behavior Tree snapshot JSON file."));
			return;
		}

		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonInput);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: invalid JSON | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotInvalidJson", "The selected file is not a valid Behavior Tree snapshot JSON file."));
			return;
		}

		FString SnapshotType;
		if (!RootObject->TryGetStringField(TEXT("snapshotType"), SnapshotType) || SnapshotType != TEXT("AuraBehaviorTreeSnapshot"))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: unsupported snapshot type | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotUnsupportedType", "The selected JSON file is not an Aura Behavior Tree snapshot export."));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* PackageFiles = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("packageFiles"), PackageFiles) || PackageFiles == nullptr || PackageFiles->IsEmpty())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: packageFiles array is missing | File='%s'"), *InputFilename);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotMissingPackageFiles", "The snapshot JSON does not contain any embedded package files."));
			return;
		}

		TArray<UPackage*> LoadedPackagesToReload;

		for (const TSharedPtr<FJsonValue>& PackageFileValue : *PackageFiles)
		{
			const TSharedPtr<FJsonObject>* PackageFileObject = nullptr;
			if (!PackageFileValue.IsValid() || !PackageFileValue->TryGetObject(PackageFileObject) || PackageFileObject == nullptr || !PackageFileObject->IsValid())
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: malformed package file entry | File='%s'"), *InputFilename);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotMalformedPackageEntry", "A package file entry in the snapshot JSON is malformed."));
				return;
			}

			FString RelativePath;
			FString ContentBase64;
			if (!(*PackageFileObject)->TryGetStringField(TEXT("projectRelativePath"), RelativePath) || !(*PackageFileObject)->TryGetStringField(TEXT("contentBase64"), ContentBase64))
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: incomplete package file entry | File='%s'"), *InputFilename);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotIncompletePackageEntry", "A package file entry in the snapshot JSON is incomplete."));
				return;
			}

			TArray<uint8> FileBytes;
			if (!FBase64::Decode(ContentBase64, FileBytes))
			{
				UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: base64 decode error | File='%s' | Target='%s'"), *InputFilename, *RelativePath);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BehaviorTreeSnapshotDecodeFailure", "Failed to decode embedded package data from the snapshot JSON."));
				return;
			}

			const FString TargetPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath);
			IFileManager& FileManager = IFileManager::Get();
			FileManager.MakeDirectory(*FPaths::GetPath(TargetPath), true);

			if (FileManager.FileExists(*TargetPath) && FileManager.IsReadOnly(*TargetPath))
			{
				const bool bClearedReadOnly = FileManager.Delete(*TargetPath, false, true, true);
				UE_LOG(LogAuraEditor, Display, TEXT("Target package was read-only; attempted delete before restore | Target='%s' | Deleted=%s"),
					*TargetPath,
					bClearedReadOnly ? TEXT("true") : TEXT("false"));
			}

			if (!FFileHelper::SaveArrayToFile(FileBytes, *TargetPath, &FileManager, FILEWRITE_EvenIfReadOnly))
			{
				const bool bTargetExists = FileManager.FileExists(*TargetPath);
				const bool bTargetReadOnly = bTargetExists && FileManager.IsReadOnly(*TargetPath);

				UE_LOG(LogAuraEditor, Error, TEXT("Behavior Tree import failed: could not write package file | Target='%s' | Exists=%s | ReadOnly=%s"),
					*TargetPath,
					bTargetExists ? TEXT("true") : TEXT("false"),
					bTargetReadOnly ? TEXT("true") : TEXT("false"));

				FMessageDialog::Open(
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("BehaviorTreeSnapshotWritePackageFailure", "Failed to write restored package file:\n{0}\n\nIf this asset is source-controlled, check it out or make it writable, then retry. If the asset is open in another process, close that process and retry."),
						FText::FromString(TargetPath)));
				return;
			}

			FString LongPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(TargetPath, LongPackageName))
			{
				if (UPackage* LoadedPackage = FindPackage(nullptr, *LongPackageName))
				{
					LoadedPackagesToReload.AddUnique(LoadedPackage);
				}
			}
		}

		if (!LoadedPackagesToReload.IsEmpty())
		{
			FText ReloadErrorMessage;
			const bool bReloadedAnyPackages = UPackageTools::ReloadPackages(
				LoadedPackagesToReload,
				ReloadErrorMessage,
				EReloadPackagesInteractionMode::AssumePositive);

			if (!bReloadedAnyPackages)
			{
				UE_LOG(LogAuraEditor, Warning, TEXT("Behavior Tree snapshot imported, but package reload did not complete | Error='%s'"), *ReloadErrorMessage.ToString());
				FMessageDialog::Open(
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("BehaviorTreeSnapshotImportReloadWarning", "Behavior Tree package files were restored from JSON, but reload did not complete:\n{0}\n\nUse Asset Actions -> Reload on the restored asset, or restart the editor."),
						ReloadErrorMessage));
				return;
			}
		}

		UE_LOG(LogAuraEditor, Display, TEXT("Behavior Tree snapshot imported successfully | File='%s'"), *InputFilename);
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("BehaviorTreeSnapshotImportSuccess", "Behavior Tree package files were restored from the JSON snapshot and any loaded packages were reloaded from disk."));
	}

	FText GetDedicatedServerMenuLabel() const
	{
#if PLATFORM_MAC
		return LOCTEXT("StartDedicatedServerLabel", "Launch Dedicated Server");
#else
		return LOCTEXT("StartDedicatedServerLabel", "Launch StartDedicatedServer");
#endif
	}

	FText GetDedicatedServerMenuTooltip() const
	{
#if PLATFORM_MAC
		return LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.command from the project root in Terminal.");
#else
		return LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.bat from the project root.");
#endif
	}

	FText GetBuildClientMenuLabel() const
	{
#if PLATFORM_MAC
		return LOCTEXT("BuildClientLabel", "Build Mac Client");
#else
		return LOCTEXT("BuildClientLabel", "Build Windows Client");
#endif
	}

	FText GetBuildClientMenuTooltip() const
	{
#if PLATFORM_MAC
		return LOCTEXT("BuildClientTooltip", "Build and package the Mac Shipping game client with cooked content in Terminal.");
#else
		return LOCTEXT("BuildClientTooltip", "Build and package the Windows Shipping game client with cooked content in a visible console window.");
#endif
	}

	bool RegisterOnMenuPath(const TCHAR* MenuPath)
	{
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus probe | Path='%s'"), MenuPath);

		if (UToolMenu* ToolBarMenu = UToolMenus::Get()->FindMenu(MenuPath))
		{
			FToolMenuSection& Section = ToolBarMenu->FindOrAddSection("Play");

			FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
				"StartDedicatedServer",
				FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnStartDedicatedServerClicked)),
				LOCTEXT("StartDedicatedServerLabel", "Dedicated Server"),
				GetDedicatedServerMenuTooltip(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));

			Section.AddEntry(Entry);
			UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus button registered | Path='%s' | Section='Play'"), MenuPath);
			return true;
		}

		UE_LOG(LogAuraEditor, Warning, TEXT("ToolMenus menu not found | Path='%s'"), MenuPath);
		return false;
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UE_LOG(LogAuraEditor, Display, TEXT("RegisterMenus invoked | ToolMenuUIEnabled=%s"), UToolMenus::IsToolMenuUIEnabled() ? TEXT("true") : TEXT("false"));

		const bool bRegisteredOnPlayToolBar = RegisterOnMenuPath(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));
		const bool bRegisteredOnRootToolBar = RegisterOnMenuPath(TEXT("LevelEditor.LevelEditorToolBar"));
		UE_LOG(LogAuraEditor, Display, TEXT("RegisterMenus result | PlayToolBar=%s | RootToolBar=%s"),
			bRegisteredOnPlayToolBar ? TEXT("true") : TEXT("false"),
			bRegisteredOnRootToolBar ? TEXT("true") : TEXT("false"));

		if (!bRegisteredOnPlayToolBar && !bRegisteredOnRootToolBar)
		{
			UE_LOG(LogAuraEditor, Error, TEXT("RegisterMenus failed: no known ToolMenus path accepted the button."));
		}
	}

	bool LaunchProjectScript(const FString& RelativeScriptPath, const FText& MissingScriptDialogText, const FString& FailureDialogText, const FString& SuccessLogLabel) const
	{
		const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeScriptPath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved project script path | Label='%s' | Path='%s'"), *SuccessLogLabel, *ScriptPath);

		if (!FPaths::FileExists(ScriptPath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Launch aborted: script file does not exist | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, MissingScriptDialogText);
			return false;
		}

		return LaunchInVisibleConsole(
			FString::Printf(TEXT("\"%s\""), *ScriptPath),
			FailureDialogText,
			SuccessLogLabel);
	}

	bool LaunchInVisibleConsole(const FString& CommandToRun, const FString& FailureDialogText, const FString& SuccessLogLabel) const
	{
#if PLATFORM_WINDOWS
		const FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
		const FString Executable = CmdExe.IsEmpty() ? TEXT("cmd.exe") : CmdExe;
		const FString Params = FString::Printf(TEXT("/k \"%s\""), *CommandToRun);
		const bool bLaunchDetached = false;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessId = 0;

		UE_LOG(LogAuraEditor, Display, TEXT("Launching process | Label='%s' | Executable='%s' | Params='%s' | WorkingDir='%s' | Detached=%s | Hidden=%s | ReallyHidden=%s"),
			*SuccessLogLabel,
			*Executable,
			*Params,
			*FPaths::ProjectDir(),
			bLaunchDetached ? TEXT("true") : TEXT("false"),
			bLaunchHidden ? TEXT("true") : TEXT("false"),
			bLaunchReallyHidden ? TEXT("true") : TEXT("false"));

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*Executable,
			*Params,
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			&ProcessId,
			0,
			*FPaths::ProjectDir(),
			nullptr);

		if (!ProcHandle.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("CreateProc failed | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FailureDialogText));
			return false;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("CreateProc succeeded | Label='%s' | PID=%u"), *SuccessLogLabel, ProcessId);
		FPlatformProcess::CloseProc(ProcHandle);
		UE_LOG(LogAuraEditor, Display, TEXT("Process handle closed after successful launch | Label='%s'"), *SuccessLogLabel);
		return true;
#elif PLATFORM_MAC
		const FString Executable = TEXT("/usr/bin/open");
		const FString Params = FString::Printf(TEXT("-a Terminal %s"), *CommandToRun);
		const bool bLaunchDetached = true;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessId = 0;

		UE_LOG(LogAuraEditor, Display, TEXT("Launching process | Label='%s' | Executable='%s' | Params='%s' | WorkingDir='%s' | Detached=%s | Hidden=%s | ReallyHidden=%s"),
			*SuccessLogLabel,
			*Executable,
			*Params,
			*FPaths::ProjectDir(),
			bLaunchDetached ? TEXT("true") : TEXT("false"),
			bLaunchHidden ? TEXT("true") : TEXT("false"),
			bLaunchReallyHidden ? TEXT("true") : TEXT("false"));

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*Executable,
			*Params,
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			&ProcessId,
			0,
			*FPaths::ProjectDir(),
			nullptr);

		if (!ProcHandle.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("CreateProc failed | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FailureDialogText));
			return false;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("CreateProc succeeded | Label='%s' | PID=%u"), *SuccessLogLabel, ProcessId);
		FPlatformProcess::CloseProc(ProcHandle);
		UE_LOG(LogAuraEditor, Display, TEXT("Process handle closed after successful launch | Label='%s'"), *SuccessLogLabel);
		return true;
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Launch blocked: non-Windows platform | Label='%s'"), *SuccessLogLabel);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnsupportedLaunchAction", "This action is not supported on this platform."));
		return false;
#endif
	}

	void OnStartDedicatedServerClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Dedicated server toolbar button clicked"));

#if PLATFORM_WINDOWS
		LaunchProjectScript(
			TEXT("StartDedicatedServer.bat"),
			FText::Format(
				LOCTEXT("StartDedicatedServerMissingWindows", "Could not find StartDedicatedServer.bat at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("StartDedicatedServer.bat")))),
			TEXT("Failed to launch StartDedicatedServer.bat."),
			TEXT("StartDedicatedServer"));
#elif PLATFORM_MAC
		LaunchProjectScript(
			TEXT("StartDedicatedServer.command"),
			FText::Format(
				LOCTEXT("StartDedicatedServerMissingMac", "Could not find StartDedicatedServer.command at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("StartDedicatedServer.command")))),
			TEXT("Failed to launch StartDedicatedServer.command."),
			TEXT("StartDedicatedServer"));
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Launch blocked: unsupported platform"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("StartDedicatedServerUnsupported", "Dedicated server launch is not supported on this platform."));
#endif
	}

	void OnBuildClientClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Build client menu option clicked"));

#if PLATFORM_WINDOWS
		const FString RunUATBatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat"));
		const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		const FString ArchiveDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Build/WindowsNoEditor"));

		UE_LOG(LogAuraEditor, Display, TEXT("Resolved RunUAT path: '%s'"), *RunUATBatPath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved project file path: '%s'"), *ProjectFilePath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved archive directory: '%s'"), *ArchiveDirectory);

		if (!FPaths::FileExists(RunUATBatPath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Build client aborted: RunUAT.bat does not exist"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("BuildWindowsClientMissingRunUATBat", "Could not find Unreal RunUAT.bat at:\n{0}"),
					FText::FromString(RunUATBatPath)));
			return;
		}

		if (!FPaths::FileExists(ProjectFilePath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Build client aborted: project file does not exist"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("BuildWindowsClientMissingProject", "Could not find the project file at:\n{0}"),
					FText::FromString(ProjectFilePath)));
			return;
		}

		const FString BuildCommand = FString::Printf(
			TEXT("\"%s\" BuildCookRun -project=\"%s\" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -package -archive -archivedirectory=\"%s\" -pak -iostore -prereqs -target=Aura"),
			*RunUATBatPath,
			*ProjectFilePath,
			*ArchiveDirectory);

		LaunchInVisibleConsole(
			BuildCommand,
			TEXT("Failed to launch the Windows Shipping client package build."),
			TEXT("BuildWindowsClient"));
#elif PLATFORM_MAC
		LaunchProjectScript(
			TEXT("BuildMacClient.command"),
			FText::Format(
				LOCTEXT("BuildMacClientMissingScript", "Could not find BuildMacClient.command at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("BuildMacClient.command")))),
			TEXT("Failed to launch the Mac Shipping client package build."),
			TEXT("BuildMacClient"));
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Build client blocked: unsupported platform"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BuildClientUnsupported", "Building the client from this menu is not supported on this platform."));
#endif
	}

	TSharedPtr<FExtender> ToolbarExtender;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAuraEditorModule, AuraEditor)
