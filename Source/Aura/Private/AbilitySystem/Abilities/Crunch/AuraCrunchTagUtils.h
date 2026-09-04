// Copyright Druid Mechanics

#pragma once

#include "GameplayTagContainer.h"

namespace AuraCrunchTags
{
	inline FGameplayTag Request(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
}
