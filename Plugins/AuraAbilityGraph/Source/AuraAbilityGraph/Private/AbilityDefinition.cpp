// Copyright Druid Mechanics

#include "AbilityDefinition.h"
#include "AbilityNodeRegistry.h"
#include "AuraAbilityGraphLogChannels.h"
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
            DebuffChance = FCString::Atof(*Child->GetAttribute(TEXT("debuffChance")));
            DebuffDamage = FCString::Atof(*Child->GetAttribute(TEXT("debuffDamage")));
            DebuffDuration = FCString::Atof(*Child->GetAttribute(TEXT("debuffDuration")));
            DebuffFrequency = FCString::Atof(*Child->GetAttribute(TEXT("debuffFrequency")));
            DeathImpulseMagnitude = FCString::Atof(*Child->GetAttribute(TEXT("deathImpulseMagnitude")));
            KnockbackForceMagnitude = FCString::Atof(*Child->GetAttribute(TEXT("knockbackForceMagnitude")));
            KnockbackChance = FCString::Atof(*Child->GetAttribute(TEXT("knockbackChance")));
        }
        else if (Tag == TEXT("montage"))
        {
            const FString MontagePath = Child->GetAttribute(TEXT("path"));
            if (!MontagePath.IsEmpty())
            {
                Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
            }
            const FString EventTagStr = Child->GetAttribute(TEXT("eventTag"));
            if (!EventTagStr.IsEmpty())
            {
                MontageEventTag = FGameplayTag::RequestGameplayTag(FName(*EventTagStr), false);
            }
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
