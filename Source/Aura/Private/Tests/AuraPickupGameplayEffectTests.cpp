#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AuraAttributeGameplayEffect.h"
#include "AuraGameplayTags.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraPickupGameplayEffectNativeTest,
	"Aura.Buffs.NativeGameplayEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraPickupGameplayEffectNativeTest::RunTest(const FString& Parameters)
{
	const UAuraPickupGameplayEffect* Instant = GetDefault<UAuraPickupGameplayEffect>();
	const UAuraPickupGameplayEffect_Duration* Duration = GetDefault<UAuraPickupGameplayEffect_Duration>();
	const UAuraPickupGameplayEffect_DurationDelayed* DurationDelayed = GetDefault<UAuraPickupGameplayEffect_DurationDelayed>();
	const UAuraPickupGameplayEffect_Infinite* Infinite = GetDefault<UAuraPickupGameplayEffect_Infinite>();

	TestEqual(TEXT("Instant policy"), Instant->DurationPolicy, EGameplayEffectDurationType::Instant);
	TestEqual(TEXT("Duration policy"), Duration->DurationPolicy, EGameplayEffectDurationType::HasDuration);
	TestTrue(TEXT("Duration executes on application"), Duration->bExecutePeriodicEffectOnApplication);
	TestFalse(TEXT("Delayed duration waits for first period"), DurationDelayed->bExecutePeriodicEffectOnApplication);
	TestEqual(TEXT("Infinite policy"), Infinite->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TestEqual(TEXT("Pickup GE exposes every supported player attribute"), Instant->Modifiers.Num(), 20);

	UWorld* TestWorld = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (UWorld* Candidate = WorldContext.World())
		{
			TestWorld = Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("A world is available for the GAS fixture"), TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AActor* OwnerActor = TestWorld->SpawnActor<AActor>(SpawnParameters);
	if (!TestNotNull(TEXT("Transient GAS owner spawned"), OwnerActor))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(OwnerActor);
	ASC->RegisterComponent();
	UAuraAttributeSet* AttributeSet = NewObject<UAuraAttributeSet>(ASC);
	ASC->AddAttributeSetSubobject(AttributeSet);
	ASC->InitAbilityActorInfo(OwnerActor, OwnerActor);
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetMaxHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), 10.f);

	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle InstantSpec = ASC->MakeOutgoingSpec(UAuraPickupGameplayEffect::StaticClass(), 1.f, Context);
	TestTrue(TEXT("Instant pickup spec created"), InstantSpec.IsValid());
	if (InstantSpec.IsValid())
	{
		for (const FGameplayModifierInfo& Modifier : Instant->Modifiers)
		{
			UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
				InstantSpec,
				Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag,
				0.f);
		}
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			InstantSpec,
			FAuraGameplayTags::Get().Attributes_Vital_Health,
			50.f);
		ASC->ApplyGameplayEffectSpecToSelf(*InstantSpec.Data);
		TestEqual(TEXT("Health SetByCaller applies"), AttributeSet->GetHealth(), 60.f);
	}

	const FGameplayEffectSpecHandle DurationSpec = ASC->MakeOutgoingSpec(UAuraPickupGameplayEffect_Duration::StaticClass(), 1.f, Context);
	TestTrue(TEXT("Duration pickup spec created"), DurationSpec.IsValid());
	if (DurationSpec.IsValid())
	{
		DurationSpec.Data->SetDuration(4.f, false);
		DurationSpec.Data->Period = 0.1f;
		TestEqual(TEXT("Duration override"), DurationSpec.Data->GetDuration(), 4.f);
		TestEqual(TEXT("Period override"), DurationSpec.Data->Period, 0.1f);
	}

	const FGameplayEffectSpecHandle InfiniteSpec = ASC->MakeOutgoingSpec(UAuraPickupGameplayEffect_Infinite::StaticClass(), 1.f, Context);
	TestTrue(TEXT("Infinite pickup spec created"), InfiniteSpec.IsValid());
	if (InfiniteSpec.IsValid())
	{
		for (const FGameplayModifierInfo& Modifier : Infinite->Modifiers)
		{
			UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
				InfiniteSpec,
				Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag,
				0.f);
		}
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*InfiniteSpec.Data);
		TestTrue(TEXT("Infinite pickup returns an active handle"), Handle.IsValid());
		TestNotNull(TEXT("Infinite pickup persists"), ASC->GetActiveGameplayEffect(Handle));
		ASC->RemoveActiveGameplayEffect(Handle);
		TestNull(TEXT("Infinite pickup can be removed"), ASC->GetActiveGameplayEffect(Handle));
	}

	OwnerActor->Destroy();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraPickupGameplayEffectConfigTest,
	"Aura.Buffs.GameplayEffectsConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraPickupGameplayEffectConfigTest::RunTest(const FString& Parameters)
{
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/GameplayEffects.json"));
	FString JsonContent;
	if (!TestTrue(TEXT("GameplayEffects.json loads"), FFileHelper::LoadFileToString(JsonContent, *ConfigPath)))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!TestTrue(TEXT("GameplayEffects.json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Pickups = nullptr;
	if (!TestTrue(TEXT("pickupEffects object exists"), Root->TryGetObjectField(TEXT("pickupEffects"), Pickups) && Pickups && Pickups->IsValid()))
	{
		return false;
	}

	for (const TCHAR* RequiredName : { TEXT("healthPotion"), TEXT("manaPotion"), TEXT("healthCrystal"), TEXT("manaCrystal"), TEXT("fireArea"), TEXT("testAttribute") })
	{
		TestTrue(FString::Printf(TEXT("pickupEffects contains %s"), RequiredName), (*Pickups)->HasField(RequiredName));
	}

	const TSharedPtr<FJsonObject>* FireArea = nullptr;
	if ((*Pickups)->TryGetObjectField(TEXT("fireArea"), FireArea) && FireArea && FireArea->IsValid())
	{
		TestEqual(TEXT("FireArea is infinite"), (*FireArea)->GetStringField(TEXT("duration")), FString(TEXT("infinite")));
		TestEqual(TEXT("FireArea tick magnitude"), (*FireArea)->GetNumberField(TEXT("health")), -5.0);
		TestEqual(TEXT("FireArea period"), (*FireArea)->GetNumberField(TEXT("period")), 1.0);
	}

	const TSharedPtr<FJsonObject>* TestAttribute = nullptr;
	if ((*Pickups)->TryGetObjectField(TEXT("testAttribute"), TestAttribute) && TestAttribute && TestAttribute->IsValid())
	{
		const TSharedPtr<FJsonObject>* Attributes = nullptr;
		if (TestTrue(TEXT("testAttribute has attributes"), (*TestAttribute)->TryGetObjectField(TEXT("attributes"), Attributes) && Attributes && Attributes->IsValid()))
		{
			TestEqual(TEXT("Resilience magnitude"), (*Attributes)->GetNumberField(TEXT("Attributes.Primary.Resilience")), 15.0);
		}
	}

	return true;
}

#endif
