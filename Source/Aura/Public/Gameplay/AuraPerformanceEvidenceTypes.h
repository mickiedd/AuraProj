// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"

struct FAuraPerformanceEvidence
{
	FString CandidateIdentity;
	FString BaselineIdentity;
	FString HardwareFingerprint;
	FString SettingsFingerprint;
	FString TraceSha256;
	int32 ServerHz = 0;
	int32 WarmupSeconds = 0;
	int32 SampleSeconds = 0;
	int32 Repeats = 0;
	double ServerGameP95Ms = 0;
	double ServerGameP99Ms = 0;
	double SchedulingP95Ms = 0;
	double ClientFrameP95Ms = 0;
	double ClientFrameP99Ms = 0;
	double OutboundKiBPerSecond = 0;
	double OutboundGrowthPercent = 0;
	double MemoryGrowthPercent = 0;
	int32 CompletedCycles = 0;
	int32 ActorCountBefore = 0;
	int32 ActorCountAfter = 0;
	int32 TimerCountBefore = 0;
	int32 TimerCountAfter = 0;
	bool bRendered = false;
	bool bReplicationParity = false;
	bool bGameplaySemanticParity = false;
};

/** Fail-closed Day 58 evidence validator. It validates measurements; it never manufactures them. */
class AURA_API FAuraPerformanceEvidenceValidator final
{
public:
	static bool Validate(const FAuraPerformanceEvidence& Evidence, FString& OutError);
};
