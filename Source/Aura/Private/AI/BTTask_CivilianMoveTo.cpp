// Copyright Druid Mechanics

#include "AI/BTTask_CivilianMoveTo.h"

UBTTask_CivilianMoveTo::UBTTask_CivilianMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Move Civilian To Destination");
	BlackboardKey.SelectedKeyName = TEXT("Destination");
	AcceptableRadius = FValueOrBBKey_Float(45.f);
	bAllowStrafe = FValueOrBBKey_Bool(true);
	bAllowPartialPath = FValueOrBBKey_Bool(true);
	bRequireNavigableEndLocation = FValueOrBBKey_Bool(true);
	bProjectGoalLocation = FValueOrBBKey_Bool(true);
	bReachTestIncludesAgentRadius = FValueOrBBKey_Bool(true);
}
