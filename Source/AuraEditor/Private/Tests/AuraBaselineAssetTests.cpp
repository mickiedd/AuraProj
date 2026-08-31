#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Sound/SoundCue.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraBaselineSoundCueGuidTest,
	"Aura.RoleBattle.Day41.Assets.SoundCueGraphGuids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraBaselineSoundCueGuidTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/MilitaryWeapDark/Sound/Rifle/Rifle_ImpactSurface_Cue.Rifle_ImpactSurface_Cue"),
		TEXT("/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue")
	};
	for (const TCHAR* Path : Paths)
	{
		USoundCue* Cue = LoadObject<USoundCue>(nullptr, Path);
		if (!TestNotNull(Path, Cue)) continue;
		UEdGraph* Graph = Cue->SoundCueGraph;
		if (!TestNotNull(TEXT("Persisted sound graph"), Graph)) continue;
		TestTrue(TEXT("Sound graph is nonempty"), !Graph->Nodes.IsEmpty());
		TestNotNull(TEXT("Playable sound root retained"), Cue->FirstNode.Get());
		TSet<FGuid> Guids;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!TestNotNull(TEXT("Persisted graph node"), Node)) continue;
			TestTrue(TEXT("Graph node has a valid persisted GUID"), Node->NodeGuid.IsValid());
			TestFalse(TEXT("Graph node GUID is unique within its cue"), Guids.Contains(Node->NodeGuid));
			Guids.Add(Node->NodeGuid);
		}
	}
	return !HasAnyErrors();
}

#endif
