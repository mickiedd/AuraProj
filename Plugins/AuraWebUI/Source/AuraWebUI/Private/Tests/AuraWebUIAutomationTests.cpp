// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "SocketSubsystem.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UI/WebUI/WebUIWebSocketServer.h"

namespace AuraWebUITestsPrivate
{
	class FSocketGuard
	{
	public:
		FSocketGuard(ISocketSubsystem* InSubsystem, FSocket* InSocket)
			: Subsystem(InSubsystem)
			, Socket(InSocket)
		{
		}

		~FSocketGuard()
		{
			if (Socket)
			{
				Socket->Close();
				if (Subsystem)
				{
					Subsystem->DestroySocket(Socket);
				}
			}
		}

		FSocket* Get() const { return Socket; }

	private:
		ISocketSubsystem* Subsystem = nullptr;
		FSocket* Socket = nullptr;
	};

	bool SendAll(FSocket* Socket, const uint8* Data, int32 NumBytes)
	{
		int32 Offset = 0;
		while (Offset < NumBytes)
		{
			int32 Sent = 0;
			if (!Socket || !Socket->Send(Data + Offset, NumBytes - Offset, Sent) || Sent <= 0)
			{
				return false;
			}
			Offset += Sent;
		}
		return true;
	}

	bool SendTextFrame(FSocket* Socket, const FString& Message)
	{
		FTCHARToUTF8 Utf8(*Message);
		if (Utf8.Length() > 125)
		{
			return false;
		}

		const uint8 Mask[4] = { 0x13, 0x37, 0xA5, 0x5A };
		TArray<uint8> Frame;
		Frame.Reserve(Utf8.Length() + 6);
		Frame.Add(0x81);
		Frame.Add(static_cast<uint8>(0x80 | Utf8.Length()));
		Frame.Append(Mask, UE_ARRAY_COUNT(Mask));
		for (int32 Index = 0; Index < Utf8.Length(); ++Index)
		{
			Frame.Add(static_cast<uint8>(Utf8.Get()[Index]) ^ Mask[Index % UE_ARRAY_COUNT(Mask)]);
		}
		return SendAll(Socket, Frame.GetData(), Frame.Num());
	}

	bool ContainsAscii(const TArray<uint8>& Bytes, const ANSICHAR* Needle)
	{
		const int32 NeedleLength = FCStringAnsi::Strlen(Needle);
		if (NeedleLength <= 0 || Bytes.Num() < NeedleLength)
		{
			return false;
		}

		for (int32 Start = 0; Start <= Bytes.Num() - NeedleLength; ++Start)
		{
			bool bMatches = true;
			for (int32 Offset = 0; Offset < NeedleLength; ++Offset)
			{
				if (Bytes[Start + Offset] != static_cast<uint8>(Needle[Offset]))
				{
					bMatches = false;
					break;
				}
			}
			if (bMatches)
			{
				return true;
			}
		}
		return false;
	}

	bool PumpUntil(
		TFunctionRef<void()> TickServer,
		FSocket* Client,
		TArray<uint8>& OutReceived,
		TFunctionRef<bool(const TArray<uint8>&)> Predicate,
		int32 MaxTicks = 250)
	{
		for (int32 TickIndex = 0; TickIndex < MaxTicks; ++TickIndex)
		{
			TickServer();

			uint32 PendingBytes = 0;
			while (Client && Client->HasPendingData(PendingBytes) && PendingBytes > 0)
			{
				const int32 PreviousSize = OutReceived.Num();
				OutReceived.SetNumUninitialized(PreviousSize + static_cast<int32>(PendingBytes));
				int32 BytesRead = 0;
				if (!Client->Recv(OutReceived.GetData() + PreviousSize, static_cast<int32>(PendingBytes), BytesRead) || BytesRead <= 0)
				{
					OutReceived.SetNum(PreviousSize);
					break;
				}
				OutReceived.SetNum(PreviousSize + BytesRead);
			}

			if (Predicate(OutReceived))
			{
				return true;
			}
			FPlatformProcess::Sleep(0.001f);
		}
		return Predicate(OutReceived);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebUIWebSocketLoopbackTest,
	"AuraWebUI.Plugin.WebSocketLoopback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebUIWebSocketLoopbackTest::RunTest(const FString& Parameters)
{
	using namespace AuraWebUITestsPrivate;

	FWebUIWebSocketServer Server;
	const uint32 TestPort = 18766;
	if (!TestTrue(TEXT("WebSocket server starts on the loopback test port"), Server.Start(TestPort)))
	{
		return false;
	}

	bool bConnected = false;
	bool bCommandReceived = false;
	FString ReceivedCommand;
	Server.SetConnectionHandler([&bConnected](bool bIsConnected)
	{
		bConnected = bIsConnected;
	});
	Server.SetMessageHandler([&](const FString& Message)
	{
		if (Message.Contains(TEXT("\"command\":\"ping\"")))
		{
			bCommandReceived = true;
			ReceivedCommand = Message;
			Server.BroadcastText(TEXT("{\"type\":\"event\",\"event\":\"automation_ack\",\"payload\":{\"ok\":true}}"));
		}
	});

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!TestNotNull(TEXT("Socket subsystem is available"), SocketSubsystem))
	{
		return false;
	}

	FSocket* RawClient = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("AuraWebUIAutomationClient"), false);
	FSocketGuard ClientGuard(SocketSubsystem, RawClient);
	if (!TestNotNull(TEXT("Loopback test client socket is created"), RawClient))
	{
		return false;
	}

	TSharedRef<FInternetAddr> ServerAddress = SocketSubsystem->CreateInternetAddr();
	bool bValidIp = false;
	ServerAddress->SetIp(TEXT("127.0.0.1"), bValidIp);
	ServerAddress->SetPort(static_cast<int32>(TestPort));
	if (!TestTrue(TEXT("Loopback address is valid"), bValidIp)
		|| !TestTrue(TEXT("Browser test client connects to the native server"), RawClient->Connect(*ServerAddress)))
	{
		return false;
	}
	RawClient->SetNonBlocking(true);

	const FString Handshake = FString::Printf(
		TEXT("GET / HTTP/1.1\r\n")
		TEXT("Host: 127.0.0.1:%d\r\n")
		TEXT("Upgrade: websocket\r\n")
		TEXT("Connection: Upgrade\r\n")
		TEXT("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
		TEXT("Sec-WebSocket-Version: 13\r\n\r\n"),
		TestPort);
	FTCHARToUTF8 HandshakeUtf8(*Handshake);
	if (!TestTrue(TEXT("Browser-style HTTP upgrade request is sent"), SendAll(RawClient, reinterpret_cast<const uint8*>(HandshakeUtf8.Get()), HandshakeUtf8.Length())))
	{
		return false;
	}

	TArray<uint8> ReceivedBytes;
	if (!TestTrue(TEXT("Native server completes the WebSocket upgrade"), PumpUntil([&Server]() { Server.Tick(); }, RawClient, ReceivedBytes, [](const TArray<uint8>& Bytes)
	{
		return ContainsAscii(Bytes, "101 Switching Protocols");
	})))
	{
		return false;
	}
	TestTrue(TEXT("Native server emits its hello frame"), ContainsAscii(ReceivedBytes, "\"type\":\"hello\""));
	TestTrue(TEXT("Connection callback reports the browser client"), bConnected);
	TestEqual(TEXT("One browser client is connected"), Server.GetClientCount(), 1);

	const FString Command = TEXT("{\"type\":\"command\",\"command\":\"ping\",\"payload\":{\"source\":\"automation\"}}");
	if (!TestTrue(TEXT("Browser-style masked JSON command is sent"), SendTextFrame(RawClient, Command)))
	{
		return false;
	}

	const bool bAckReceived = PumpUntil([&Server]() { Server.Tick(); }, RawClient, ReceivedBytes, [](const TArray<uint8>& Bytes)
	{
		return ContainsAscii(Bytes, "automation_ack");
	});
	TestTrue(TEXT("Native server receives the unmasked JSON command"), bCommandReceived);
	TestTrue(TEXT("Native server broadcasts a JSON response frame"), bAckReceived);
	TestTrue(TEXT("Command payload reaches native code intact"), ReceivedCommand.Contains(TEXT("\"source\":\"automation\"")));
	Server.Stop();
	TestFalse(TEXT("Server stops cleanly after the browser disconnects"), Server.IsRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebUIBridgeProtocolTest,
	"AuraWebUI.Plugin.BridgeProtocol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebUIBridgeProtocolTest::RunTest(const FString& Parameters)
{
	using namespace AuraWebUITestsPrivate;

	UWorld::InitializationValues InitializationValues;
	InitializationValues
		.CreatePhysicsScene(false)
		.ShouldSimulatePhysics(false)
		.EnableTraceCollision(false)
		.CreateNavigation(false)
		.CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		FName(TEXT("AuraWebUIAutomationWorld")),
		nullptr,
		true,
		ERHIFeatureLevel::SM5,
		&InitializationValues,
		false);
	if (!TestNotNull(TEXT("Game world fixture is created"), World))
	{
		return false;
	}

	UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>();
	if (!TestNotNull(TEXT("Game world owns the Web UI bridge subsystem"), Bridge))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!TestTrue(TEXT("Bridge starts its loopback server"), Bridge->IsServerRunning()))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Bridge uses its documented default port"), Bridge->GetServerPort(), UWebUIBridgeSubsystem::DefaultPort);

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FSocket* RawClient = SocketSubsystem ? SocketSubsystem->CreateSocket(NAME_Stream, TEXT("AuraWebUIBridgeAutomationClient"), false) : nullptr;
	FSocketGuard ClientGuard(SocketSubsystem, RawClient);
	if (!TestNotNull(TEXT("Bridge protocol client socket is created"), RawClient))
	{
		World->DestroyWorld(false);
		return false;
	}

	TSharedRef<FInternetAddr> ServerAddress = SocketSubsystem->CreateInternetAddr();
	bool bValidIp = false;
	ServerAddress->SetIp(TEXT("127.0.0.1"), bValidIp);
	ServerAddress->SetPort(Bridge->GetServerPort());
	if (!TestTrue(TEXT("Bridge loopback address is valid"), bValidIp)
		|| !TestTrue(TEXT("Client connects to the public bridge endpoint"), RawClient->Connect(*ServerAddress)))
	{
		World->DestroyWorld(false);
		return false;
	}
	RawClient->SetNonBlocking(true);

	const FString Handshake = FString::Printf(
		TEXT("GET / HTTP/1.1\r\n")
		TEXT("Host: 127.0.0.1:%d\r\n")
		TEXT("Upgrade: websocket\r\n")
		TEXT("Connection: Upgrade\r\n")
		TEXT("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
		TEXT("Sec-WebSocket-Version: 13\r\n\r\n"),
		Bridge->GetServerPort());
	FTCHARToUTF8 HandshakeUtf8(*Handshake);
	if (!TestTrue(TEXT("Bridge protocol upgrade request is sent"), SendAll(RawClient, reinterpret_cast<const uint8*>(HandshakeUtf8.Get()), HandshakeUtf8.Length())))
	{
		World->DestroyWorld(false);
		return false;
	}

	TArray<uint8> ReceivedBytes;
	const auto TickBridge = [&Bridge]() { Bridge->Tick(0.016f); };
	if (!TestTrue(TEXT("Bridge completes the WebSocket upgrade"), PumpUntil(TickBridge, RawClient, ReceivedBytes, [](const TArray<uint8>& Bytes)
	{
		return ContainsAscii(Bytes, "101 Switching Protocols");
	})))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!TestTrue(TEXT("Ready command frame is sent"), SendTextFrame(RawClient, TEXT("{\"type\":\"ready\"}"))))
	{
		World->DestroyWorld(false);
		return false;
	}
	const bool bReadyResponse = PumpUntil(TickBridge, RawClient, ReceivedBytes, [](const TArray<uint8>& Bytes)
	{
		return ContainsAscii(Bytes, "bridge_ready");
	});
	TestTrue(TEXT("Bridge answers ready with bridge_ready"), bReadyResponse);

	if (!TestTrue(TEXT("Ping command frame is sent"), SendTextFrame(RawClient, TEXT("{\"type\":\"command\",\"command\":\"ping\",\"payload\":{}}"))))
	{
		World->DestroyWorld(false);
		return false;
	}
	const bool bPongResponse = PumpUntil(TickBridge, RawClient, ReceivedBytes, [](const TArray<uint8>& Bytes)
	{
		return ContainsAscii(Bytes, "pong");
	});
	TestTrue(TEXT("Bridge answers ping with pong"), bPongResponse);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebUIPluginContentContractTest,
	"AuraWebUI.Plugin.ContentContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebUIPluginContentContractTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AuraWebUI"));
	if (!TestTrue(TEXT("AuraWebUI plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString DescriptorPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("AuraWebUI.uplugin"));
	const FString HtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/index.html"));
	const FString LoadingHtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/loading.html"));
	const FString LoginHtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/login.html"));
	const FString ConfigEditorHtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/config-editor.html"));
	const TArray<FString> HudPanelNames = {
		TEXT("hud-left-top.html"),
		TEXT("hud-right-top.html"),
		TEXT("hud-bottom.html"),
		TEXT("hud-interaction.html")
	};
	const FString SkillPanelHtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/skill-panel.html"));
	const FString DefaultEnginePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini"));
	const TArray<FString> ManualSkillIconNames = {
		TEXT("empty.png"),
		TEXT("firebolt.png"),
		TEXT("gunfire.png"),
		TEXT("electrocute.png"),
		TEXT("fireblast.png"),
		TEXT("arcaneshards.png"),
		TEXT("haloofprotection.png"),
		TEXT("lifesiphon.png"),
		TEXT("manasiphon.png")
	};
	const FString WebUIWidgetSourcePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/AuraWebUI/Private/UI/WebUI/WebUIWidget.cpp"));
	FString Descriptor;
	FString Html;
	FString LoadingHtml;
	FString LoginHtml;
	FString ConfigEditorHtml;
	TArray<FString> HudPanelHtml;
	FString SkillPanelHtml;
	FString DefaultEngine;
	TestTrue(TEXT("Plugin descriptor exists"), FFileHelper::LoadFileToString(Descriptor, *DescriptorPath));
	TestTrue(TEXT("Packaged sample page exists in the plugin"), FFileHelper::LoadFileToString(Html, *HtmlPath));
	TestTrue(TEXT("Loading page exists in the plugin"), FFileHelper::LoadFileToString(LoadingHtml, *LoadingHtmlPath));
	TestTrue(TEXT("Login page exists in the plugin"), FFileHelper::LoadFileToString(LoginHtml, *LoginHtmlPath));
	TestTrue(TEXT("Config editor page exists in the plugin"), FFileHelper::LoadFileToString(ConfigEditorHtml, *ConfigEditorHtmlPath));
	for (const FString& PanelName : HudPanelNames)
	{
		FString PanelHtml;
		const FString PanelPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI"), PanelName);
		TestTrue(FString::Printf(TEXT("Gameplay HUD panel exists: %s"), *PanelName), FFileHelper::LoadFileToString(PanelHtml, *PanelPath));
		HudPanelHtml.Add(MoveTemp(PanelHtml));
	}
	TestTrue(TEXT("Skill panel page exists in the plugin"), FFileHelper::LoadFileToString(SkillPanelHtml, *SkillPanelHtmlPath));
	TestTrue(TEXT("Project game-map configuration is readable"), FFileHelper::LoadFileToString(DefaultEngine, *DefaultEnginePath));
	for (const FString& IconName : ManualSkillIconNames)
	{
		TArray<uint8> IconBytes;
		const FString IconPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/skill-icons"), IconName);
		TestTrue(FString::Printf(TEXT("Manual skill PNG exists: %s"), *IconName), FFileHelper::LoadFileToArray(IconBytes, *IconPath) && IconBytes.Num() > 8);
		if (IconBytes.Num() >= 8)
		{
			const uint8 PngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
			bool bPngSignatureMatches = true;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(PngSignature); ++Index)
			{
				bPngSignatureMatches &= IconBytes[Index] == PngSignature[Index];
			}
			TestTrue(FString::Printf(TEXT("Manual skill asset is a PNG: %s"), *IconName), bPngSignatureMatches);
		}
	}
	FString WebUIWidgetSource;
	if (TestTrue(TEXT("Web UI widget implementation is readable"), FFileHelper::LoadFileToString(WebUIWidgetSource, *WebUIWidgetSourcePath)))
	{
		TestTrue(TEXT("Web UI widget configures native browser transparency before Slate creation"),
			WebUIWidgetSource.Contains(TEXT("bSupportsTransparency"))
			&& WebUIWidgetSource.Contains(TEXT("SetPropertyValue_InContainer")));
	}
	TestTrue(TEXT("Plugin declares WebBrowserWidget dependency"), Descriptor.Contains(TEXT("WebBrowserWidget")));
	TestTrue(TEXT("Sample page uses the native WebSocket URL placeholder"), Html.Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("Sample page sends explicit command messages"), Html.Contains(TEXT("type: 'command'")));
	TestTrue(TEXT("Sample page covers the ping command"), Html.Contains(TEXT("command('ping'")));
	TestTrue(TEXT("Sample page covers the state command"), Html.Contains(TEXT("command('get_state'")));
	TestTrue(TEXT("Loading page uses the native WebSocket URL placeholder"), LoadingHtml.Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("Loading page consumes loading_progress events"), LoadingHtml.Contains(TEXT("loading_progress")));
	TestTrue(TEXT("Loading page renders a progress bar"), LoadingHtml.Contains(TEXT("role=\"progressbar\"")));
	TestTrue(TEXT("Loading page renders loading status"), LoadingHtml.Contains(TEXT("Preparing your world")));
	TestTrue(TEXT("Login page uses the native WebSocket URL placeholder"), LoginHtml.Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("Login page sends level selection commands"), LoginHtml.Contains(TEXT("login_select_level")));
	TestTrue(TEXT("Login page sends connect commands"), LoginHtml.Contains(TEXT("login_connect")));
	TestTrue(TEXT("Login page sends role selection commands"), LoginHtml.Contains(TEXT("login_select_role")));
	TestTrue(TEXT("Login page includes the selected role in connect payloads"), LoginHtml.Contains(TEXT("roleId: selectedRoleId")));
	TestTrue(TEXT("Login page consumes login state events"), LoginHtml.Contains(TEXT("login_state")));
	TestTrue(TEXT("Login page consumes role lists from login state"), LoginHtml.Contains(TEXT("payload.roles")));
	TestTrue(TEXT("Login page consumes login status events"), LoginHtml.Contains(TEXT("login_status")));
	TestTrue(TEXT("Login page renders a server selector"), LoginHtml.Contains(TEXT("id=\"levels\"")));
	TestTrue(TEXT("Login page renders a role selector"), LoginHtml.Contains(TEXT("id=\"roles\"")));
	TestTrue(TEXT("Login page renders a connect button"), LoginHtml.Contains(TEXT("id=\"connect\"")));
	TestTrue(TEXT("Login page renders live status text"), LoginHtml.Contains(TEXT("aria-live=\"polite\"")));
	TestTrue(TEXT("Config editor uses the native WebSocket URL placeholder"), ConfigEditorHtml.Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("Config editor requests the native config list"), ConfigEditorHtml.Contains(TEXT("send(\"config_list\")")));
	TestTrue(TEXT("Config editor reads files through the bridge"), ConfigEditorHtml.Contains(TEXT("send(\"config_read\"")));
	TestTrue(TEXT("Config editor saves with an optimistic version"), ConfigEditorHtml.Contains(TEXT("send(\"config_save\"")) && ConfigEditorHtml.Contains(TEXT("version: state.version")));
	TestTrue(TEXT("Config editor includes structured and raw editing modes"), ConfigEditorHtml.Contains(TEXT("Structured view")) && ConfigEditorHtml.Contains(TEXT("Raw JSON")));
	TestTrue(TEXT("Every gameplay HUD panel uses the native WebSocket URL placeholder"), HudPanelHtml.Num() == HudPanelNames.Num() && HudPanelHtml[0].Contains(TEXT("__AURA_WEBSOCKET_URL__")) && HudPanelHtml[1].Contains(TEXT("__AURA_WEBSOCKET_URL__")) && HudPanelHtml[2].Contains(TEXT("__AURA_WEBSOCKET_URL__")) && HudPanelHtml[3].Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("HUD panels report readiness on their shared bridge"), HudPanelHtml[0].Contains(TEXT("hud_ready")) && HudPanelHtml[1].Contains(TEXT("hud_ready")) && HudPanelHtml[2].Contains(TEXT("hud_ready")) && HudPanelHtml[3].Contains(TEXT("hud_ready")));
	TestTrue(TEXT("Bottom HUD panel owns only vitals and skill presentation"), HudPanelHtml[2].Contains(TEXT("hud_vitals")) && HudPanelHtml[2].Contains(TEXT("skill_panel_ability")) && !HudPanelHtml[2].Contains(TEXT("hud_interaction")));
	TestTrue(TEXT("Dedicated interaction HUD owns target preview and clear behavior"), HudPanelHtml[3].Contains(TEXT("hud_interaction")) && HudPanelHtml[3].Contains(TEXT("hud_interaction_cleared")) && HudPanelHtml[3].Contains(TEXT("state.selectedOption=-1")));
	TestTrue(TEXT("Bottom HUD skill strip reserves enough height for its tiles and labels"), HudPanelHtml[2].Contains(TEXT("align-items:center")) && HudPanelHtml[2].Contains(TEXT("min-height:104px")));
	TestTrue(TEXT("Right-top HUD panel owns menus and their controller commands"), HudPanelHtml[1].Contains(TEXT("hud_attribute_upgrade")) && HudPanelHtml[1].Contains(TEXT("hud_spell_slot")) && HudPanelHtml[1].Contains(TEXT("hud_quit_confirm")) && HudPanelHtml[1].Contains(TEXT("hud_menu_closed")));
	TestTrue(TEXT("Left-top HUD panel owns progress and transient messages"), HudPanelHtml[0].Contains(TEXT("hud_progress")) && HudPanelHtml[0].Contains(TEXT("hud_message")) && HudPanelHtml[0].Contains(TEXT("hud_level_up")));
	TestTrue(TEXT("Left-top HUD explains role, life, battle, population, and persistence state"), HudPanelHtml[0].Contains(TEXT("hud_role_state")) && HudPanelHtml[0].Contains(TEXT("hud_battle_state")) && HudPanelHtml[0].Contains(TEXT("populationSummary")) && HudPanelHtml[0].Contains(TEXT("persistentProfile")) && HudPanelHtml[0].Contains(TEXT("lifeState")));
	TestTrue(TEXT("Interaction HUD explains civilian activity and combat protection"), HudPanelHtml[3].Contains(TEXT("p.activity")) && HudPanelHtml[3].Contains(TEXT("Combat protected")) && HudPanelHtml[3].Contains(TEXT("hud_merchant_open")));
	TestTrue(TEXT("Right-top HUD exposes owner-only economy and merchant results"), HudPanelHtml[1].Contains(TEXT("hud_economy")) && HudPanelHtml[1].Contains(TEXT("hud_merchant")) && HudPanelHtml[1].Contains(TEXT("hud_merchant_buy")) && HudPanelHtml[1].Contains(TEXT("hud_merchant_result")) && HudPanelHtml[1].Contains(TEXT("inventoryModal")));
	TestTrue(TEXT("Gameplay HUD panels are transparent and not full-screen HTML surfaces"), HudPanelHtml[0].Contains(TEXT("background:transparent")) && HudPanelHtml[1].Contains(TEXT("background:transparent")) && HudPanelHtml[2].Contains(TEXT("background:transparent")) && HudPanelHtml[3].Contains(TEXT("background:transparent")) && !HudPanelHtml[0].Contains(TEXT("position:fixed;inset:0")) && !HudPanelHtml[1].Contains(TEXT("position:fixed;inset:0")) && !HudPanelHtml[2].Contains(TEXT("position:fixed;inset:0")) && !HudPanelHtml[3].Contains(TEXT("position:fixed;inset:0")));
	TestTrue(TEXT("Gameplay HUD does not synthesize level LMB clicks from browser pointer events"), !HudPanelHtml[0].Contains(TEXT("startGameplayLmb")) && !HudPanelHtml[1].Contains(TEXT("startGameplayLmb")) && !HudPanelHtml[2].Contains(TEXT("startGameplayLmb")) && !HudPanelHtml[3].Contains(TEXT("startGameplayLmb")) && HudPanelHtml[2].Contains(TEXT("skill_ability_pressed")) && HudPanelHtml[2].Contains(TEXT("skill_ability_released")));
	TestTrue(TEXT("Gameplay HUD uses modern ability icon treatments"), HudPanelHtml[2].Contains(TEXT("skill::before")) && HudPanelHtml[2].Contains(TEXT("skill.offensive")) && HudPanelHtml[2].Contains(TEXT("saturate(1.14)")) && HudPanelHtml[2].Contains(TEXT("skill.equipped")));
	TestTrue(TEXT("Gameplay HUD renders PNG icons supplied by the native bridge"), HudPanelHtml[2].Contains(TEXT("img.src=info.icon")) && HudPanelHtml[2].Contains(TEXT("img.className=info.icon?'show':''")));
	TestTrue(TEXT("Gameplay HUD uses the canonical native passive input tags"), HudPanelHtml[2].Contains(TEXT("const passiveSlots=['InputTag.Passive.1','InputTag.Passive.2'];")) && !HudPanelHtml[2].Contains(TEXT("InputTag.Passive_1")) && !HudPanelHtml[2].Contains(TEXT("InputTag.Passive_2")));
	TestTrue(TEXT("Gameplay HUD does not forward input from passive abilities"), HudPanelHtml[2].Contains(TEXT("const isPassive=passiveSlots.includes(slot)||type.includes('passive');")) && HudPanelHtml[2].Contains(TEXT("if(info.abilityTag&&!isPassive)")));
	TestTrue(TEXT("Editor startup returns to the WebUI login flow"), DefaultEngine.Contains(TEXT("EditorStartupMap=/Game/Maps/Login.Login")));
	TestTrue(TEXT("Game startup returns to the WebUI login flow"), DefaultEngine.Contains(TEXT("GameDefaultMap=/Game/Maps/Login.Login")));
	TestTrue(TEXT("Skill panel uses the native WebSocket URL placeholder"), SkillPanelHtml.Contains(TEXT("__AURA_WEBSOCKET_URL__")));
	TestTrue(TEXT("Skill panel requests a startup replay when the page is ready"), SkillPanelHtml.Contains(TEXT("skill_panel_ready")));
	TestTrue(TEXT("Skill panel renders ability-info events"), SkillPanelHtml.Contains(TEXT("skill_panel_ability")));
	TestTrue(TEXT("Skill panel sends press, held, and release commands"),
		SkillPanelHtml.Contains(TEXT("skill_ability_pressed"))
		&& SkillPanelHtml.Contains(TEXT("skill_ability_held"))
		&& SkillPanelHtml.Contains(TEXT("skill_ability_released")));
	TestTrue(TEXT("Skill panel forwards held input while the pointer remains down"), SkillPanelHtml.Contains(TEXT("setInterval(() => send('skill_ability_held'")));
	TestTrue(TEXT("Skill HUD renders health and mana values and progress fills"),
		SkillPanelHtml.Contains(TEXT("id=\"healthValue\""))
		&& SkillPanelHtml.Contains(TEXT("id=\"healthFill\""))
		&& SkillPanelHtml.Contains(TEXT("id=\"manaValue\""))
		&& SkillPanelHtml.Contains(TEXT("id=\"manaFill\""))
		&& SkillPanelHtml.Contains(TEXT("hud_vitals")));
	TestTrue(TEXT("Skill HUD renders the Attributes, Spells, and Close controls"),
		SkillPanelHtml.Contains(TEXT("hud_attributes_clicked"))
		&& SkillPanelHtml.Contains(TEXT("hud_spells_clicked"))
		&& SkillPanelHtml.Contains(TEXT("hud_close_clicked")));
	TestTrue(TEXT("Skill HUD is bounded by responsive CSS instead of a full-screen loading layout"),
		SkillPanelHtml.Contains(TEXT("grid-template-columns: minmax(205px, 1fr)"))
		&& SkillPanelHtml.Contains(TEXT("background: transparent"))
		&& !SkillPanelHtml.Contains(TEXT("min-height: 100vh")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebUIRoleBattleHUDContractTest,
	"AuraWebUI.Plugin.RoleBattleHUDContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebUIRoleBattleHUDContractTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AuraWebUI"));
	if (!TestTrue(TEXT("AuraWebUI plugin is discoverable for the Role/Battle HUD contract"), Plugin.IsValid()))
	{
		return false;
	}

	const FString HUDDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI"));
	FString LeftTop;
	FString RightTop;
	FString Bottom;
	FString Interaction;
	const bool bLoaded =
		TestTrue(TEXT("Role/Battle left-top HUD page is readable"), FFileHelper::LoadFileToString(LeftTop, *FPaths::Combine(HUDDirectory, TEXT("hud-left-top.html"))))
		&& TestTrue(TEXT("Role/Battle right-top HUD page is readable"), FFileHelper::LoadFileToString(RightTop, *FPaths::Combine(HUDDirectory, TEXT("hud-right-top.html"))))
		&& TestTrue(TEXT("Role/Battle bottom HUD page is readable"), FFileHelper::LoadFileToString(Bottom, *FPaths::Combine(HUDDirectory, TEXT("hud-bottom.html"))))
		&& TestTrue(TEXT("Role/Battle interaction HUD page is readable"), FFileHelper::LoadFileToString(Interaction, *FPaths::Combine(HUDDirectory, TEXT("hud-interaction.html"))));
	if (!bLoaded)
	{
		return false;
	}

	TestTrue(TEXT("Left-top consumes replicated role and life state fields"),
		LeftTop.Contains(TEXT("hud_role_state"))
		&& LeftTop.Contains(TEXT("lifeStateName"))
		&& LeftTop.Contains(TEXT("persistentProfile"))
		&& LeftTop.Contains(TEXT("combatProfile")));
	TestTrue(TEXT("Left-top consumes battle phase, event, and population lifecycle fields"),
		LeftTop.Contains(TEXT("hud_battle_state"))
		&& LeftTop.Contains(TEXT("populationActive"))
		&& LeftTop.Contains(TEXT("populationMaximum"))
		&& LeftTop.Contains(TEXT("populationPending"))
		&& LeftTop.Contains(TEXT("populationCasualties")));
	TestTrue(TEXT("Bottom HUD keeps interaction content out of the skill panel"),
		!Bottom.Contains(TEXT("hud_interaction"))
		&& !Bottom.Contains(TEXT("interaction-head")));
	TestTrue(TEXT("Interaction HUD renders orthogonal target, activity, and combat affordance fields"),
		Interaction.Contains(TEXT("p.relationshipTag"))
		&& Interaction.Contains(TEXT("p.lifeTag"))
		&& Interaction.Contains(TEXT("p.activity"))
		&& Interaction.Contains(TEXT("p.attackAllowed"))
		&& Interaction.Contains(TEXT("Combat protected"))
		&& Interaction.Contains(TEXT("hud_interaction_activate"))
		&& Interaction.Contains(TEXT("hud_interaction_cleared"))
		&& Interaction.Contains(TEXT("state.selectedOption=-1")));
	TestTrue(TEXT("Interaction HUD keeps Trade discoverability separate from Interact execution"),
		Interaction.Contains(TEXT("Interaction.Trade"))
		&& Interaction.Contains(TEXT("hud_merchant_open"))
		&& Interaction.Contains(TEXT("Shop ready"))
		&& Interaction.Contains(TEXT("hud_interaction_activate")));
	TestTrue(TEXT("Right-top renders owner economy, inventory, and merchant result state"),
		RightTop.Contains(TEXT("hud_economy"))
		&& RightTop.Contains(TEXT("inventoryModal"))
		&& RightTop.Contains(TEXT("hud_merchant"))
		&& RightTop.Contains(TEXT("hud_merchant_buy"))
		&& RightTop.Contains(TEXT("hud_merchant_result"))
		&& RightTop.Contains(TEXT("commerceResultName")));

	FString HUDSource;
	const FString HUDSourcePath = FPaths::ProjectDir() / TEXT("Source/Aura/Private/UI/HUD/AuraHUD.cpp");
	if (TestTrue(TEXT("AuraHUD source is readable for state-flow checks"), FFileHelper::LoadFileToString(HUDSource, *HUDSourcePath)))
	{
		TestTrue(TEXT("HUD binds role, currency, inventory, and purchase-result delegates"),
			HUDSource.Contains(TEXT("HandleRoleChangedForWebUI"))
			&& HUDSource.Contains(TEXT("HandleCurrencyChangedForWebUI"))
			&& HUDSource.Contains(TEXT("HandleInventoryChangedForWebUI"))
			&& HUDSource.Contains(TEXT("HandlePurchaseResultForWebUI")));
		TestTrue(TEXT("HUD clears replay caches when the browser disconnects"),
			HUDSource.Contains(TEXT("if (!bConnected)"))
			&& HUDSource.Contains(TEXT("LastRoleStatePayloadJson.Empty()"))
			&& HUDSource.Contains(TEXT("LastBattleStatePayloadJson.Empty()"))
			&& HUDSource.Contains(TEXT("LastMerchantPayloadJson.Empty()")));
		TestTrue(TEXT("HUD gates merchant opening on native Trade and active merchant state"),
			HUDSource.Contains(TEXT("bTradeAvailable"))
			&& HUDSource.Contains(TEXT("Merchant->IsMerchantActive()"))
			&& HUDSource.Contains(TEXT("Interaction_Trade")));
		TestTrue(TEXT("HUD routes purchases through the focused native interaction component"),
			HUDSource.Contains(TEXT("GetFocusedTargetActor()"))
			&& HUDSource.Contains(TEXT("RequestPurchase"))
			&& HUDSource.Contains(TEXT("GetInteractionComponent()")));
		TestTrue(TEXT("HUD clears a stale interaction preview in its separate browser panel"),
			HUDSource.Contains(TEXT("WebUI/hud-interaction.html"))
			&& HUDSource.Contains(TEXT("else if (bWebInteractionVisible) HandleTargetPreviewClearedForWebUI();"))
			&& HUDSource.Contains(TEXT("bWebInteractionVisible = false;")));
	}

	return !HasAnyErrors();
}

#endif
