// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"

struct FAuraPlaytestTelemetry
{
	TSet<int32> AcceptedRequestIds;
	int32 AcceptedAbilityEvents = 0;
	int32 AcceptedObjectiveEvents = 0;
	int32 AcceptedRewardEvents = 0;
	bool RecordAccepted(int32 RequestId, bool bAuthorityAccepted, FName EventKind);
	static bool IsPrivacySafe(bool bHasRawIdentity, bool bHasVoice, bool bHasVideo, bool bVideoConsent);
	static bool DefinitionsRemainBound(FString ExpectedHash, FString ObservedHash, bool bNativeRegressionPassed);
};
