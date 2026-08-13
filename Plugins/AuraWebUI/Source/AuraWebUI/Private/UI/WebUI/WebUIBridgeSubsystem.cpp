// Copyright Druid Mechanics

#include "UI/WebUI/WebUIBridgeSubsystem.h"

#include "JsonObjectConverter.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "UI/WebUI/WebUIWebSocketServer.h"

UWebUIBridgeSubsystem::~UWebUIBridgeSubsystem() = default;

bool UWebUIBridgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World
		&& !IsRunningDedicatedServer()
		&& World->GetNetMode() != NM_DedicatedServer
		&& (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWebUIBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Server = new FWebUIWebSocketServer();
	Server->SetMessageHandler([this](const FString& Message)
	{
		HandleMessage(Message);
	});
	Server->SetConnectionHandler([this](bool bConnected)
	{
		HandleConnectionChanged(bConnected);
	});

	StartServer(DefaultPort);
}

void UWebUIBridgeSubsystem::Deinitialize()
{
	StopServer();
	delete Server;
	Server = nullptr;
	Super::Deinitialize();
}

void UWebUIBridgeSubsystem::Tick(float DeltaTime)
{
	if (Server)
	{
		Server->Tick();
	}
}

TStatId UWebUIBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWebUIBridgeSubsystem, STATGROUP_Tickables);
}

bool UWebUIBridgeSubsystem::StartServer(int32 Port)
{
	if (!Server || Port <= 0 || Port > 65535)
	{
		return false;
	}
	if (Server->IsRunning())
	{
		return Server->GetPort() == static_cast<uint32>(Port);
	}

	const bool bStarted = Server->Start(static_cast<uint32>(Port));
	if (bStarted)
	{
		UE_LOG(LogTemp, Display, TEXT("[WebUI] WebSocket bridge listening on %s"), *GetWebSocketUrl());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] Could not bind loopback WebSocket bridge on port %d"), Port);
	}
	return bStarted;
}

void UWebUIBridgeSubsystem::StopServer()
{
	if (Server)
	{
		Server->Stop();
	}
}

bool UWebUIBridgeSubsystem::IsServerRunning() const
{
	return Server && Server->IsRunning();
}

int32 UWebUIBridgeSubsystem::GetServerPort() const
{
	return Server ? static_cast<int32>(Server->GetPort()) : 0;
}

int32 UWebUIBridgeSubsystem::GetConnectedClientCount() const
{
	return Server ? Server->GetClientCount() : 0;
}

FString UWebUIBridgeSubsystem::GetWebSocketUrl() const
{
	return FString::Printf(TEXT("ws://127.0.0.1:%d"), GetServerPort());
}

bool UWebUIBridgeSubsystem::SendEvent(const FString& EventName, const FString& PayloadJson)
{
	if (!Server || !Server->IsRunning())
	{
		return false;
	}

	TSharedPtr<FJsonValue> PayloadValue;
	const FString NormalizedPayload = PayloadJson.TrimStartAndEnd().IsEmpty() ? TEXT("{}") : PayloadJson.TrimStartAndEnd();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NormalizedPayload);
	if (!FJsonSerializer::Deserialize(Reader, PayloadValue) || !PayloadValue.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] SendEvent rejected invalid payload for '%s'"), *EventName);
		return false;
	}

	TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("type"), TEXT("event"));
	Message->SetStringField(TEXT("event"), EventName);
	Message->SetField(TEXT("payload"), PayloadValue);

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Message, Writer);
	Writer->Close();
	Server->BroadcastText(Serialized);
	return true;
}

bool UWebUIBridgeSubsystem::SendRawJson(const FString& MessageJson)
{
	if (!Server || !Server->IsRunning())
	{
		return false;
	}

	TSharedPtr<FJsonObject> MessageObject;
	if (!ParseJsonObject(MessageJson, MessageObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] SendRawJson rejected non-object JSON"));
		return false;
	}

	Server->BroadcastText(MessageJson.TrimStartAndEnd());
	return true;
}

void UWebUIBridgeSubsystem::HandleMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> Object;
	if (!ParseJsonObject(Message, Object))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] Ignoring malformed JSON command"));
		return;
	}

	FString Type;
	if (!Object->TryGetStringField(TEXT("type"), Type))
	{
		return;
	}
	if (Type == TEXT("ready"))
	{
		SendEvent(TEXT("bridge_ready"), MakeServerStateJson(*this));
		return;
	}

	if (Type != TEXT("command"))
	{
		return;
	}

	FString Command;
	if (!Object->TryGetStringField(TEXT("command"), Command))
	{
		return;
	}
	const TSharedPtr<FJsonValue>* PayloadValue = Object->Values.Find(TEXT("payload"));
	const FString PayloadJson = PayloadValue ? SerializeJsonValue(*PayloadValue) : TEXT("{}");

	if (Command == TEXT("ping"))
	{
		const FString PongPayload = FString::Printf(TEXT("{\"utc\":\"%s\"}"), *FDateTime::UtcNow().ToIso8601());
		SendEvent(TEXT("pong"), PongPayload);
	}
	else if (Command == TEXT("get_state"))
	{
		SendEvent(TEXT("state"), MakeServerStateJson(*this));
	}

	OnCommand.Broadcast(Command, PayloadJson);
}

void UWebUIBridgeSubsystem::HandleConnectionChanged(bool bConnected)
{
	OnConnectionChanged.Broadcast(bConnected);
	UE_LOG(LogTemp, Display, TEXT("[WebUI] Browser connection %s (clients=%d)"), bConnected ? TEXT("opened") : TEXT("closed"), GetConnectedClientCount());
}

bool UWebUIBridgeSubsystem::ParseJsonObject(const FString& Message, TSharedPtr<FJsonObject>& OutObject)
{
	const FString Trimmed = Message.TrimStartAndEnd();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
	return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

FString UWebUIBridgeSubsystem::SerializeJsonValue(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return TEXT("null");
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Value, TEXT(""), Writer);
	Writer->Close();
	return Serialized;
}

FString UWebUIBridgeSubsystem::MakeServerStateJson(const UWebUIBridgeSubsystem& Bridge)
{
	return FString::Printf(
		TEXT("{\"port\":%d,\"clients\":%d,\"level\":\"%s\"}"),
		Bridge.GetServerPort(),
		Bridge.GetConnectedClientCount(),
		*GetNameSafe(Bridge.GetWorld()));
}
