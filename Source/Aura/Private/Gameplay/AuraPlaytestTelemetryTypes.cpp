// Copyright Druid Mechanics
#include "Gameplay/AuraPlaytestTelemetryTypes.h"
bool FAuraPlaytestTelemetry::RecordAccepted(int32 RequestId,bool bAccepted,FName Kind){if(RequestId<=0||!bAccepted||AcceptedRequestIds.Contains(RequestId))return false;if(Kind==TEXT("Ability"))++AcceptedAbilityEvents;else if(Kind==TEXT("Objective"))++AcceptedObjectiveEvents;else if(Kind==TEXT("Reward"))++AcceptedRewardEvents;else return false;AcceptedRequestIds.Add(RequestId);return true;}
bool FAuraPlaytestTelemetry::IsPrivacySafe(bool Raw,bool Voice,bool Video,bool Consent){return !Raw&&!Voice&&(!Video||Consent);}
bool FAuraPlaytestTelemetry::DefinitionsRemainBound(FString Expected,FString Observed,bool Passed){return !Expected.IsEmpty()&&Expected==Observed&&Passed;}
