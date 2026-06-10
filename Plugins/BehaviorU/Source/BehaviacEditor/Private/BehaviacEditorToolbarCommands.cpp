// BehaviacEditorToolbarCommands.cpp

#include "BehaviacEditorToolbarCommands.h"

#define LOCTEXT_NAMESPACE "FBehaviacEditorModule"

void FBehaviacEditorToolbarCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenBTEditor,
		"BT Editor",
		"Open the Behaviac Behavior Tree web editor",
		EUserInterfaceActionType::Button,
		FInputChord());

	//UI_COMMAND(
	//	ReimportSelectedBT,
	//	"Reimport BT",
	//	"Reimport the selected Behaviac Behavior Tree asset from its XML source file",
	//	EUserInterfaceActionType::Button,
	//	FInputChord());

	UI_COMMAND(
		ToggleDebugServer,
		"BT Client Debug",
		"启动/停止行为树调试 WebSocket 服务器（端口 17654）。\n激活后可在 Behaviac Editor 中实时查看黑板数据。",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());

	UI_COMMAND(
		ToggleServerDebugServer,
		"BT Server Debug",
		"通过 ServerExec 在服务端启动/停止行为树调试 WebSocket 服务器。\n需要 PIE 或连接到 DS 的会话处于运行状态。",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
