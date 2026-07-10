// BehaviorUEditorToolbarCommands — UI_COMMAND for the BT editor toolbar button.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "BehaviorUEditorStyle.h"

class FBehaviorUEditorToolbarCommands : public TCommands<FBehaviorUEditorToolbarCommands>
{
public:
	FBehaviorUEditorToolbarCommands()
		: TCommands<FBehaviorUEditorToolbarCommands>(
			  TEXT("BehaviorUEditor"),
			  NSLOCTEXT("Contexts", "BehaviorUEditor", "BehaviorU Editor"),
			  NAME_None,
			  FBehaviorUEditorStyle::GetStyleSetName())
	{
	}

	void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenBTEditor;
	TSharedPtr<FUICommandInfo> ReimportSelectedBT;
	// 切换行为树调试 WebSocket 服务器（本地客户端，Start/Stop）
	TSharedPtr<FUICommandInfo> ToggleDebugServer;
	// 切换服务端行为树调试服务器（通过 ServerExec 转发命令到 DS/PIE Server）
	TSharedPtr<FUICommandInfo> ToggleServerDebugServer;
};
