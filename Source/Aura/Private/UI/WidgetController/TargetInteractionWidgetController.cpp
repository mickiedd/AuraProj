// Copyright Druid Mechanics

#include "UI/WidgetController/TargetInteractionWidgetController.h"

#include "AuraGameplayTags.h"
#include "Combat/AuraCombatRules.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraTargetableInterface.h"

bool UTargetInteractionWidgetController::BuildPreview(const AActor* Observer, AActor* Target, FAuraTargetDescriptor& OutDescriptor) const
{
	OutDescriptor = FAuraTargetDescriptor();
	const IAuraTargetableInterface* Targetable = Cast<IAuraTargetableInterface>(Target);
	if (!IsValid(Observer) || !Targetable) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	OutDescriptor.KindTag = Targetable->GetAuraTargetKind();
	OutDescriptor.DisplayName = Targetable->GetAuraTargetDisplayName();
	OutDescriptor.Health = Targetable->GetAuraTargetHealth();
	OutDescriptor.MaxHealth = Targetable->GetAuraTargetMaxHealth();
	Targetable->GetAuraInteractionOptions(Observer, OutDescriptor.InteractionOptions);
	const EAuraCombatLifeState Life = Targetable->GetAuraTargetState() ? Targetable->GetAuraTargetState()->GetLifeState() : EAuraCombatLifeState::Alive;
	switch (Life)
	{
	case EAuraCombatLifeState::Alive: OutDescriptor.LifeTag = Tags.Target_Life_Alive; break;
	case EAuraCombatLifeState::Dying: OutDescriptor.LifeTag = Tags.Target_Life_Dying; break;
	case EAuraCombatLifeState::Dead: OutDescriptor.LifeTag = Tags.Target_Life_Dead; break;
	case EAuraCombatLifeState::Respawning: OutDescriptor.LifeTag = Tags.Target_Life_Respawning; break;
	}
	FAuraCombatRuleContext Context;
	const FAuraCombatRuleResult Relationship = FAuraCombatRules::GetRelationship(Observer, Target, Context);
	switch (Relationship.Relationship)
	{
	case EAuraCombatRelationship::Hostile: OutDescriptor.RelationshipTag = Tags.Target_Relationship_Hostile; break;
	case EAuraCombatRelationship::Friendly: OutDescriptor.RelationshipTag = Tags.Target_Relationship_Friendly; break;
	case EAuraCombatRelationship::Protected: OutDescriptor.RelationshipTag = Tags.Target_Relationship_Protected; break;
	default: OutDescriptor.RelationshipTag = Tags.Target_Relationship_Neutral; break;
	}
	const FAuraCombatRuleResult Attack = FAuraCombatRules::CanDamage(Observer, Target, Context);
	OutDescriptor.bLocallyAttackAllowed = Attack.bCanDamage;
	OutDescriptor.AttackRejectionReason = Attack.RejectionReason;
	return OutDescriptor.IsValid();
}
