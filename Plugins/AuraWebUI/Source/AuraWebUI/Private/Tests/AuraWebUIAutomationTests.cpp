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
	const FString SkillPanelHtmlPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/skill-panel.html"));
	const FString WebUIWidgetSourcePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/AuraWebUI/Private/UI/WebUI/WebUIWidget.cpp"));
	FString Descriptor;
	FString Html;
	FString LoadingHtml;
	FString LoginHtml;
	FString ConfigEditorHtml;
	FString SkillPanelHtml;
	TestTrue(TEXT("Plugin descriptor exists"), FFileHelper::LoadFileToString(Descriptor, *DescriptorPath));
	TestTrue(TEXT("Packaged sample page exists in the plugin"), FFileHelper::LoadFileToString(Html, *HtmlPath));
	TestTrue(TEXT("Loading page exists in the plugin"), FFileHelper::LoadFileToString(LoadingHtml, *LoadingHtmlPath));
	TestTrue(TEXT("Login page exists in the plugin"), FFileHelper::LoadFileToString(LoginHtml, *LoginHtmlPath));
	TestTrue(TEXT("Config editor page exists in the plugin"), FFileHelper::LoadFileToString(ConfigEditorHtml, *ConfigEditorHtmlPath));
	TestTrue(TEXT("Skill panel page exists in the plugin"), FFileHelper::LoadFileToString(SkillPanelHtml, *SkillPanelHtmlPath));
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

#endif
