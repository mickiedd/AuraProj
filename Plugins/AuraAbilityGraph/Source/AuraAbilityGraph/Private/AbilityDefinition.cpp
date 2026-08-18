// Copyright Druid Mechanics

#include "AbilityDefinition.h"
#include "AbilityNodeRegistry.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AuraAbilityGraphLogChannels.h"
#include "AuraAbilityTypes.h"
#include "AuraGameplayTags.h"
#include "Misc/LexFromString.h"
#include "Nodes/AbilityActionNode.h"
#include "XmlFile.h"

static UAuraAbilityActionNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer);

static bool ParseXMLFloatAttribute(const FXmlNode* Node, const TCHAR* AttributeName, float& OutValue, bool bRequired)
{
    const FString Text = Node ? Node->GetAttribute(AttributeName) : FString();
    if (Text.IsEmpty())
    {
        if (bRequired)
        {
            UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] XML element '%s' is missing numeric attribute '%s'"),
                Node ? *Node->GetTag() : TEXT("<null>"), AttributeName);
            return false;
        }
        return true;
    }
    if (!LexTryParseString(OutValue, *Text) || !FMath::IsFinite(OutValue))
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] XML element '%s' has invalid numeric attribute '%s=%s'"),
            Node ? *Node->GetTag() : TEXT("<null>"), AttributeName, *Text);
        return false;
    }
    return true;
}

static bool IsKnownAbilityType(const FGameplayTag& Tag, const FAuraGameplayTags& GameplayTags)
{
    return Tag.MatchesTagExact(GameplayTags.Abilities_Type_Offensive)
        || Tag.MatchesTagExact(GameplayTags.Abilities_Type_Passive)
        || Tag.MatchesTagExact(GameplayTags.Abilities_Type_None);
}

static bool IsKnownDamageType(const FGameplayTag& Tag, const FAuraGameplayTags& GameplayTags)
{
    return Tag.MatchesTagExact(GameplayTags.Damage_Fire)
        || Tag.MatchesTagExact(GameplayTags.Damage_Lightning)
        || Tag.MatchesTagExact(GameplayTags.Damage_Arcane)
        || Tag.MatchesTagExact(GameplayTags.Damage_Physical);
}

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

    const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
    AbilityName = FName(*Root->GetAttribute(TEXT("name")));
    if (AbilityName.IsNone())
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] XML ability is missing a name"));
        return false;
    }
    AbilityTags.Reset();
    AbilityTag = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("abilityTag"))), false);
    if (!AbilityTag.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has no registered abilityTag"), *AbilityName.ToString());
        return false;
    }
    AbilityTags.AddTag(AbilityTag);

    TArray<FString> AdditionalTags;
    Root->GetAttribute(TEXT("abilityTags")).ParseIntoArray(AdditionalTags, TEXT(","), true);
    for (FString& TagString : AdditionalTags)
    {
        TagString.TrimStartAndEndInline();
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
        if (!Tag.IsValid())
        {
            UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has unregistered abilityTags entry '%s'"), *AbilityName.ToString(), *TagString);
            return false;
        }
        AbilityTags.AddTag(Tag);
    }
    InputTag = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("inputTag"))), false);
    if (!Root->GetAttribute(TEXT("inputTag")).IsEmpty() && !InputTag.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an unregistered inputTag"), *AbilityName.ToString());
        return false;
    }
    AbilityType = FGameplayTag::RequestGameplayTag(FName(*Root->GetAttribute(TEXT("type"))), false);
    if (!AbilityType.IsValid() || !IsKnownAbilityType(AbilityType, GameplayTags))
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an unsupported type"), *AbilityName.ToString());
        return false;
    }

    CooldownTag = FGameplayTag();
    CooldownDuration.Value = 0.f;
    ManaCost = 0.f;
    DamageEffectClass = UAuraDamageGameplayEffect::StaticClass();
    DamageType = FGameplayTag();
    Damage.Value = 0.f;
    DebuffChance = 20.f;
    DebuffDamage = 5.f;
    DebuffDuration = 5.f;
    DebuffFrequency = 1.f;
    DeathImpulseMagnitude = 10000.f;
    KnockbackForceMagnitude = 10000.f;
    KnockbackChance = 0.f;
    RootNode = nullptr;

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
            CooldownTag = FGameplayTag::RequestGameplayTag(FName(*CooldownTagStr), false);
            if (!CooldownTag.IsValid())
            {
                UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an invalid cooldown tag"), *AbilityName.ToString());
                return false;
            }
            if (!ParseXMLFloatAttribute(Child, TEXT("duration"), CooldownDuration.Value, true) || CooldownDuration.Value < 0.f)
            {
                UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an invalid cooldown duration"), *AbilityName.ToString());
                return false;
            }
        }
        else if (Tag == TEXT("cost"))
        {
            if (!ParseXMLFloatAttribute(Child, TEXT("mana"), ManaCost, true) || ManaCost < 0.f)
            {
                UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an invalid mana cost"), *AbilityName.ToString());
                return false;
            }
        }
        else if (Tag == TEXT("damage"))
        {
            const FString EffectClassPath = Child->GetAttribute(TEXT("effectClass"));
            if (!EffectClassPath.IsEmpty())
            {
                DamageEffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectClassPath);
                if (!DamageEffectClass)
                {
                    UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' references an invalid damage effect class '%s'"), *AbilityName.ToString(), *EffectClassPath);
                    return false;
                }
            }
            const FString DamageTypeStr = Child->GetAttribute(TEXT("type"));
            DamageType = FGameplayTag::RequestGameplayTag(FName(*DamageTypeStr), false);
            if (!DamageType.IsValid() || !IsKnownDamageType(DamageType, GameplayTags))
            {
                UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' has an invalid damage type"), *AbilityName.ToString());
                return false;
            }
            if (!ParseXMLFloatAttribute(Child, TEXT("base"), Damage.Value, false)
                || !ParseXMLFloatAttribute(Child, TEXT("debuffChance"), DebuffChance, false)
                || !ParseXMLFloatAttribute(Child, TEXT("debuffDamage"), DebuffDamage, false)
                || !ParseXMLFloatAttribute(Child, TEXT("debuffDuration"), DebuffDuration, false)
                || !ParseXMLFloatAttribute(Child, TEXT("debuffFrequency"), DebuffFrequency, false)
                || !ParseXMLFloatAttribute(Child, TEXT("deathImpulseMagnitude"), DeathImpulseMagnitude, false)
                || !ParseXMLFloatAttribute(Child, TEXT("knockbackForceMagnitude"), KnockbackForceMagnitude, false)
                || !ParseXMLFloatAttribute(Child, TEXT("knockbackChance"), KnockbackChance, false))
            {
                return false;
            }
            const FString CurveTablePath = Child->GetAttribute(TEXT("curveTable"));
            const FString CurveRow = Child->GetAttribute(TEXT("curveRow"));
            if (CurveTablePath.IsEmpty() != CurveRow.IsEmpty())
            {
                UE_LOG(LogAuraAbilityGraph, Error, TEXT("[AuraAbilityGraph] Ability '%s' must provide both curveTable and curveRow"), *AbilityName.ToString());
                return false;
            }
            if (!CurveTablePath.IsEmpty())
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

    if (!RootNode)
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
