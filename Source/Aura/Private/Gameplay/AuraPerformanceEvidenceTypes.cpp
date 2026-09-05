// Copyright Druid Mechanics
#include "Gameplay/AuraPerformanceEvidenceTypes.h"

bool FAuraPerformanceEvidenceValidator::Validate(const FAuraPerformanceEvidence& E, FString& OutError)
{
	OutError.Reset();
	auto Fail = [&OutError](const TCHAR* Reason){ OutError = Reason; return false; };
	if (E.CandidateIdentity.IsEmpty() || E.BaselineIdentity.IsEmpty() || E.CandidateIdentity == E.BaselineIdentity)
		return Fail(TEXT("PerformanceIdentityInvalid"));
	if (E.HardwareFingerprint.IsEmpty() || E.SettingsFingerprint.IsEmpty() || E.TraceSha256.Len() != 64 || !E.bRendered)
		return Fail(TEXT("PerformanceEvidenceMissing"));
	if (E.ServerHz != 30 || E.WarmupSeconds < 300 || E.SampleSeconds < 600 || E.Repeats < 3 || E.CompletedCycles < 10)
		return Fail(TEXT("PerformanceSamplingIncomplete"));
	if (!E.bReplicationParity || !E.bGameplaySemanticParity)
		return Fail(TEXT("PerformanceParityFailed"));
	if (E.ServerGameP95Ms > 25.0 || E.ServerGameP99Ms > 33.3 || E.SchedulingP95Ms > 2.0
		|| E.ClientFrameP95Ms > 22.0 || E.ClientFrameP99Ms > 33.3
		|| E.OutboundKiBPerSecond > 100.0 || E.OutboundGrowthPercent > 25.0)
		return Fail(TEXT("PerformanceBudgetExceeded"));
	if (E.ActorCountBefore != E.ActorCountAfter || E.TimerCountBefore != E.TimerCountAfter || E.MemoryGrowthPercent > 5.0)
		return Fail(TEXT("PerformanceCleanupUnbounded"));
	return true;
}
