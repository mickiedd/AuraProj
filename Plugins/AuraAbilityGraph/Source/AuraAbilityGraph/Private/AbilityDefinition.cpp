// Copyright Druid Mechanics

#include "AbilityDefinition.h"
#include "AbilityNodeRegistry.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AuraAbilityGraphLogChannels.h"
#include "AuraAbilityTypes.h"
#include "Nodes/AbilityActionNode.h"
#include "XmlFile.h"

static UAuraAbilityActionNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer);

static UAuraAbilityActionNode* CreateNodeByClassName(const FString& ClassName, UObject* Outer)
{
    if (UAuraAbilityActionNode* Node = FAuraAbilityNodeRegistry::Get().Create(ClassName, Outer))
    {
        return Node;
    }

    UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[AuraAbilityGraph] Unknown node class: %s"), *ClassName);
    return nullptr;
}

static UAuraAbilityActionNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer)
{
    if (!XmlNode)
    {
        return nullptr;
    }

    FString ClassName = XmlNode->GetAttribute(TEXT("class"));
    if (ClassName.IsEmpty())
    {
        ClassName = XmlNode->GetTag();
    }

    int32 LastColonIdx;
    if (ClassName.FindLastChar(TEXT(':'), LastColonIdx))
    {
        ClassName = ClassName.Mid(LastColonIdx + 1);
    }

    UAuraAbilityActionNode* BehaviorNode = CreateNodeByClassName(ClassName, Outer);
    if (!BehaviorNode)
    {
        return nullptr;
    }

    BehaviorNode->NodeClassName = ClassName;

    TArray<FAuraAbilityGraphProperty> Properties;
    for (const FXmlNode* PropNode : XmlNode->GetChildrenNodes())
    {
        if (PropNode->GetTag() == TEXT("property"))
        {
            Properties.Add(FAuraAbilityGraphProperty(
                PropNode->GetAttribute(TEXT("name")),
                PropNode->GetAttribute(TEXT("value"))
            ));
        }
    }

    FString IdAttr = XmlNode->GetAttribute(TEXT("id"));
    if (!IdAttr.IsEmpty())
    {
        Properties.Add(FAuraAbilityGraphProperty(TEXT("Id"), IdAttr));
    }

    BehaviorNode->LoadFromProperties(0, Properties);

    for (const FXmlNode* ChildXml : XmlNode->GetChildrenNodes())
    {
        if (ChildXml->GetTag() == TEXT("node") || ChildXml->GetTag() == TEXT("custom"))
        {
            UAuraAbilityActionNode* ChildNode = ParseNodeFromXML(ChildXml, Outer);
            if (ChildNode)
            {
                BehaviorNode->AddChild(ChildNode);
            }
        }
    }

    return BehaviorNode;
}

bool UAuraAbilityDefinition::LoadFromXML(const FString& XMLContent)
{
    SourceXML = XMLContent;

    FString Sanitized = XMLContent;
    {
        int32 PrologStart = Sanitized.Find(TEXT("<?xml"), ESearchCase::IgnoreCase);
        if (PrologStart != INDEX_NONE)
        {
            int32 PrologEnd = Sanitized.Find(TEXT("?>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PrologStart);
            if (PrologEnd != INDEX_NONE)
            {
                Sanitized.RemoveAt(PrologStart, (PrologEnd + 2) - PrologStart);
            }
        }
    }
    Sanitized = Sanitized.Replace(TEXT(">"), TEXT(">\n"));

    FXmlFile XmlFile(Sanitized, EConstructMethod::ConstructFromBuffer);
    if (!XmlFile.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[AuraAbilityGraph] Failed to parse XML content"));
        return false;
    }

    const FXmlNode* Root = XmlFile.GetRootNode();
    if (!Root)
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] XML has no root node"));
        return false;
    }

    AbilityName = FName(*Root->GetAttribute(TEXT("name")));
    AbilityTag = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("abilityTag"))), false);
    if (AbilityTag.IsValid())
    {
        AbilityTags.AddTag(AbilityTag);
    }
    TArray<FString> AdditionalTags;
    Root->GetAttribute(TEXT("abilityTags")).ParseIntoArray(AdditionalTags, TEXT(","), true);
    for (FString& TagString : AdditionalTags)
    {
        TagString.TrimStartAndEndInline();
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
        if (Tag.IsValid())
        {
            AbilityTags.AddTag(Tag);
        }
    }
    InputTag = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("inputTag"))), false);
    AbilityType = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("type"))), false);

    for (const FXmlNode* Child : Root->GetChildrenNodes())
    {
        const FString Tag = Child->GetTag();
        if (Tag == TEXT("property"))
        {
            // General properties are already handled via attributes on root
        }
        else if (Tag == TEXT("cooldown"))
        {
            const FString CooldownTagStr = Child->GetAttribute(TEXT("tag"));
            if (!CooldownTagStr.IsEmpty())
            {
                CooldownTag = FGameplayTag::RequestGameplayTag(FName(*CooldownTagStr), false);
            }
            const FString DurationStr = Child->GetAttribute(TEXT("duration"));
            if (!DurationStr.IsEmpty())
            {
                CooldownDuration.Value = FCString::Atof(*DurationStr);
            }
        }
        else if (Tag == TEXT("cost"))
        {
            const FString ManaStr = Child->GetAttribute(TEXT("mana"));
            if (!ManaStr.IsEmpty())
            {
                ManaCost = FCString::Atof(*ManaStr);
            }
        }
        else if (Tag == TEXT("damage"))
        {
            const FString EffectClassPath = Child->GetAttribute(TEXT("effectClass"));
            if (!EffectClassPath.IsEmpty())
            {
                DamageEffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectClassPath);
            }
            const FString DamageTypeStr = Child->GetAttribute(TEXT("type"));
            if (!DamageTypeStr.IsEmpty())
            {
                DamageType = FGameplayTag::RequestGameplayTag(FName(*DamageTypeStr), false);
            }
            const FString BaseStr = Child->GetAttribute(TEXT("base"));
            if (!BaseStr.IsEmpty())
            {
                Damage.Value = FCString::Atof(*BaseStr);
            }
            const FString CurveTablePath = Child->GetAttribute(TEXT("curveTable"));
            const FString CurveRow = Child->GetAttribute(TEXT("curveRow"));
            if (!CurveTablePath.IsEmpty() && !CurveRow.IsEmpty())
            {
                if (UCurveTable* CurveTable = LoadObject<UCurveTable>(nullptr, *CurveTablePath))
                {
                    Damage.Curve.CurveTable = CurveTable;
                    Damage.Curve.RowName = FName(*CurveRow);
                }
                else
                {
                    UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Failed to load damage curve table '%s'"), *CurveTablePath);
                    return false;
                }
            }
            DebuffChance = FCString::Atof(*Child->GetAttribute(TEXT("debuffChance")));
            DebuffDamage = FCString::Atof(*Child->GetAttribute(TEXT("debuffDamage")));
            DebuffDuration = FCString::Atof(*Child->GetAttribute(TEXT("debuffDuration")));
            DebuffFrequency = FCString::Atof(*Child->GetAttribute(TEXT("debuffFrequency")));
            DeathImpulseMagnitude = FCString::Atof(*Child->GetAttribute(TEXT("deathImpulseMagnitude")));
            KnockbackForceMagnitude = FCString::Atof(*Child->GetAttribute(TEXT("knockbackForceMagnitude")));
            KnockbackChance = FCString::Atof(*Child->GetAttribute(TEXT("knockbackChance")));
        }
        else if (Tag == TEXT("graph"))
        {
            const FXmlNode* FirstNode = Child->FindChildNode(TEXT("node"));
            if (FirstNode)
            {
                RootNode = ParseNodeFromXML(FirstNode, this);
            }
            else
            {
                for (const FXmlNode* GraphChild : Child->GetChildrenNodes())
                {
                    if (GraphChild->GetAttribute(TEXT("class")).Len() > 0)
                    {
                        RootNode = ParseNodeFromXML(GraphChild, this);
                        break;
                    }
                }
            }
        }
    }

    if (!RootNode && AbilityTag.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[AuraAbilityGraph] Ability %s has no <graph> node."), *AbilityTag.ToString());
    }

    return RootNode != nullptr;
}

void UAuraAbilityDefinition::BuildDamageEffectParams(FDamageEffectParams& OutParams, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, AActor* WorldContext, float AbilityLevel, const FVector& Direction, bool bIsRadialDamage, const FVector& RadialOrigin, float RadialInnerRadius, float RadialOuterRadius) const
{
    OutParams.WorldContextObject = WorldContext;
    OutParams.SourceAbilitySystemComponent = SourceASC;
    OutParams.TargetAbilitySystemComponent = TargetASC;
    OutParams.AbilityLevel = AbilityLevel;
    OutParams.DamageGameplayEffectClass = DamageEffectClass;
    OutParams.DamageType = DamageType;
    OutParams.AbilityTag = AbilityTag;
    OutParams.CombatRuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
    OutParams.CombatRuleContext.TrustedWorldContext = WorldContext;
    OutParams.CombatRuleContext.SourceActor = UAuraAbilitySystemLibrary::GetSafeAvatarActor(SourceASC);
    OutParams.CombatRuleContext.TargetActor = UAuraAbilitySystemLibrary::GetSafeAvatarActor(TargetASC);
    OutParams.CombatRuleContext.ImpactLocation = RadialOrigin;
    OutParams.BaseDamage = Damage.GetValueAtLevel(AbilityLevel);
    OutParams.DebuffChance = DebuffChance;
    OutParams.DebuffDamage = DebuffDamage;
    OutParams.DebuffDuration = DebuffDuration;
    OutParams.DebuffFrequency = DebuffFrequency;
    OutParams.DeathImpulseMagnitude = DeathImpulseMagnitude;
    OutParams.DeathImpulse = Direction * DeathImpulseMagnitude;
    OutParams.KnockbackForceMagnitude = KnockbackForceMagnitude;
    OutParams.KnockbackForce = Direction * KnockbackForceMagnitude;
    OutParams.KnockbackChance = KnockbackChance;
    OutParams.bIsRadialDamage = bIsRadialDamage;
    OutParams.RadialDamageInnerRadius = RadialInnerRadius;
    OutParams.RadialDamageOuterRadius = RadialOuterRadius;
    OutParams.RadialDamageOrigin = RadialOrigin;
}
