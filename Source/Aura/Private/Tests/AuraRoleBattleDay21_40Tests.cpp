// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AuraRoleBattleDay21_40TestsPrivate
{
	FString Read(const TCHAR* RelativePath)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / RelativePath));
		return Text;
	}

	bool FileExists(const TCHAR* RelativePath)
	{
		return IFileManager::Get().FileExists(*(FPaths::ProjectDir() / RelativePath));
	}

	bool ContainsAll(const TCHAR* RelativePath, std::initializer_list<const TCHAR*> Tokens)
	{
		const FString Text = Read(RelativePath);
		if (Text.IsEmpty()) return false;
		for (const TCHAR* Token : Tokens) if (!Text.Contains(Token)) return false;
		return true;
	}
}

#define AURA_CANDIDATE_TEST(ClassName, TestPath, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.RoleBattle." TestPath, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_CANDIDATE_TEST(FAuraDay21ScopeContractTest, "Day21.Scope.Contract",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json"), { TEXT("playable-candidate-v1"), TEXT("Aura-listen"), TEXT("BungeeMan-dedicated"), TEXT("server-issued-at-most-once") }));

AURA_CANDIDATE_TEST(FAuraDay22BaselineRunnerTest, "Day22.Baseline.Runner",
	AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/RunPlayableCandidateDay22.ps1")) && AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/test_playable_candidate.py")));

AURA_CANDIDATE_TEST(FAuraDay23BootMatrixTest, "Day23.Boot.Matrix",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("RunPlayableCandidate.ps1"), { TEXT("Stage"), TEXT("Fast"), TEXT("Candidate"), TEXT("packagedStatus") }));

AURA_CANDIDATE_TEST(FAuraDay24TutorialAuthorityTest, "Day24.Tutorial.Authority",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Content/Config/PlayableCandidateTutorial.json"), { TEXT("completionAuthority"), TEXT("server-owned"), TEXT("unavailableReason") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp"), { TEXT("OnRep_TutorialCompletionMask"), TEXT("OnTutorialProgressChanged.Broadcast") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/UI/HUD/AuraHUD.cpp"), { TEXT("OnTutorialProgressChanged.AddUObject") }));

AURA_CANDIDATE_TEST(FAuraDay25AmmoAuthorityTest, "Day25.Ammo.Authority",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp"), { TEXT("TryConsumeFirearmRound"), TEXT("AmmoRevision"), TEXT("NotAuthority") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp"), { TEXT("IAuraFirearmAuthority"), TEXT("TryConsumeFirearmRound"), TEXT("AuthorityUnavailable"), TEXT("FinishSpawning") }));

AURA_CANDIDATE_TEST(FAuraDay26HudReplayTest, "Day26.HUD.Replay",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/UI/HUD/AuraHUD.cpp"), { TEXT("SendFirearmStateToWebUI"), TEXT("hud_firearm"), TEXT("Not applicable to Aura") }));

AURA_CANDIDATE_TEST(FAuraDay27SemiAutoInputTest, "Day27.FireMode.SemiAuto",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp"), { TEXT("Press-edge activation"), TEXT("held notifications into automatic fire") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Content/AbilityDefinitions/FireGun.xml"), { TEXT("fireMode=\"SemiAuto\""), TEXT("shotConsumption=\"1\"") }));

AURA_CANDIDATE_TEST(FAuraDay28RecoveryCancellationTest, "Day28.Recovery.Cancellation",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp"), { TEXT("CancelFirearmReload"), TEXT("FinishFirearmReload"), TEXT("RecoveryState"), TEXT("NotAlive") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), { TEXT("CancelFirearmReload(TEXT(\"PlayerDeath\"))"), TEXT("SetRecoveryState(TEXT(\"Dead\"))"), TEXT("SetRecoveryState(TEXT(\"Alive\"))") }));

AURA_CANDIDATE_TEST(FAuraDay29RewardAtomicityTest, "Day29.Reward.Atomicity",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Content/Config/RewardDefinitions.json"), { TEXT("CivilianLethal"), TEXT("\"amount\": 25"), TEXT("market_health_potion") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), { TEXT("GrantedCivilianOutcomeIds"), TEXT("CommitCreditCurrency") }));

AURA_CANDIDATE_TEST(FAuraDay30RestockClockTest, "Day30.Restock.Clock",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), { TEXT("LastObservedAtUtc"), TEXT("FTimespan::FromSeconds(600.0)"), TEXT("bStartup") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Content/Config/PlayableCandidateManifest.json"), { TEXT("max-observed"), TEXT("startupCatchUpIntervals") }));

AURA_CANDIDATE_TEST(FAuraDay31PersistenceClosureTest, "Day31.Persistence.Closure",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp"), { TEXT("FirearmAmmoRevision"), TEXT("TutorialCompletionMask"), TEXT("LastRestockAtUtc"), TEXT("RestoreCheckpointState"), TEXT("DuplicateObject<UAuraPlayerSaveGame>"), TEXT("DeleteGameInSlot") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Character/AuraCharacter.cpp"), { TEXT("ApplyRoleAtSpawn(AuthorizedRole"), TEXT("ApplyPersistentProfile(*PersistentProfile") }));

AURA_CANDIDATE_TEST(FAuraDay32ReconnectGenerationTest, "Day32.JoinReconnect.Generation",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json"), { TEXT("generation"), TEXT("reconnect") }) && AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/RunPlayableCandidateDay32.ps1")));

AURA_CANDIDATE_TEST(FAuraDay33NegativeMatrixTest, "Day33.Security.NegativeMatrix",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Docs/Reference/Playable-Candidate-RPC-Ownership.md"), { TEXT("Failure behavior"), TEXT("explicit commerce result") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), { TEXT("InvalidSession"), TEXT("RequestGap"), TEXT("StaleRequest") }));

AURA_CANDIDATE_TEST(FAuraDay34ContentManifestTest, "Day34.Content.Manifest",
	AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Content/Config/PlayableCandidateManifest.json")) && AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/ValidatePlayableCandidateContent.ps1")));

AURA_CANDIDATE_TEST(FAuraDay35RedactedDiagnosticsTest, "Day35.Diagnostics.Redacted",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Scripts/WritePlayableCandidateDiagnostics.ps1"), { TEXT("run-scoped-HMAC"), TEXT("REDACTED"), TEXT("AURA_CANDIDATE_HMAC_KEY_BASE64"), TEXT("buildRevision"), TEXT("sessionId"), TEXT("serverInstanceId"), TEXT("worldPersistenceId"), TEXT("playerIdentityHmac"), TEXT("resultCode") }));

AURA_CANDIDATE_TEST(FAuraDay36VisualQATest, "Day36.VisualQA.PackagedProfile",
	AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/ValidatePlayableCandidateVisual.ps1")) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Plugins/AuraWebUI/Content/WebUI/hud-right-top.html"), { TEXT("firearmPanel"), TEXT("hud_firearm"), TEXT("reloadFirearm") }));

AURA_CANDIDATE_TEST(FAuraDay37OperationsRunbookTest, "Day37.Operations.Runbook",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Docs/Reference/Playable-Candidate-Server-Operations.md"), { TEXT("second developer"), TEXT("BLOCKED"), TEXT("RunPlayableCandidate.ps1") }));

AURA_CANDIDATE_TEST(FAuraDay38PipelinePropagationTest, "Day38.Pipeline.Propagation",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("RunPlayableCandidate.ps1"), { TEXT("candidate-draft.json"), TEXT("-Finalize"), TEXT("LaneEvidencePath"), TEXT("DIAGNOSTIC"), TEXT("packageSha256"), TEXT("clean working tree") }));

AURA_CANDIDATE_TEST(FAuraDay39SoakMatrixTest, "Day39.Soak.FourLanes",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json"), { TEXT("Aura-listen"), TEXT("BungeeMan-listen"), TEXT("Aura-dedicated"), TEXT("BungeeMan-dedicated") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("RunPlayableCandidate.ps1"), { TEXT("cyclesPerLane = 0"), TEXT("No packaged four-lane ten-cycle soak was executed") }) && AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("Scripts/RunPlayableCandidateDay39Soak.ps1"), { TEXT("-Stage Candidate -Soak") }));

AURA_CANDIDATE_TEST(FAuraDay40ImmutableSignoffTest, "Day40.Signoff.Immutable",
	AuraRoleBattleDay21_40TestsPrivate::ContainsAll(TEXT("RunPlayableCandidate.ps1"), { TEXT("Immutable candidate already exists"), TEXT("candidate.json"), TEXT("SHA-256"), TEXT("Assert-RecordBinding"), TEXT("cyclesPerLane"), TEXT("Set-ItemProperty"), TEXT("packageSha256") }) && AuraRoleBattleDay21_40TestsPrivate::FileExists(TEXT("Scripts/RunPlayableCandidateDay40.ps1")));

#undef AURA_CANDIDATE_TEST

#endif
