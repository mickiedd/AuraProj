// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

/**
 * Small game-thread WebSocket server used by the in-game Web UI.
 *
 * The browser is the client: it connects to a loopback TCP socket using the
 * browser's standard WebSocket implementation.  The server deliberately only
 * handles the subset needed for JSON text messages, ping/pong, and close.
 */
class FWebUIWebSocketServer
{
public:
	using FMessageHandler = TFunction<void(const FString& Message)>;
	using FConnectionHandler = TFunction<void(bool bConnected)>;

	FWebUIWebSocketServer();
	~FWebUIWebSocketServer();

	FWebUIWebSocketServer(const FWebUIWebSocketServer&) = delete;
	FWebUIWebSocketServer& operator=(const FWebUIWebSocketServer&) = delete;

	bool Start(uint32 Port);
	void Stop();
	void Tick();

	bool IsRunning() const { return bRunning; }
	uint32 GetPort() const { return ListenPort; }
	int32 GetClientCount() const { return Clients.Num(); }

	void BroadcastText(const FString& Message);

	void SetMessageHandler(FMessageHandler InHandler) { MessageHandler = MoveTemp(InHandler); }
	void SetConnectionHandler(FConnectionHandler InHandler) { ConnectionHandler = MoveTemp(InHandler); }

private:
	enum class EClientState : uint8
	{
		Handshaking,
		Connected
	};

	struct FClient
	{
		FSocket* Socket = nullptr;
		EClientState State = EClientState::Handshaking;
		TArray<uint8> ReceiveBuffer;
	};

	FSocket* ListenSocket = nullptr;
	TArray<FClient*> Clients;
	bool bRunning = false;
	uint32 ListenPort = 0;

	FMessageHandler MessageHandler;
	FConnectionHandler ConnectionHandler;

	void AcceptPending();
	void ReceiveFrom(FClient* Client);
	void HandleHandshake(FClient* Client);
	void HandleFrames(FClient* Client);
	void RemoveClient(int32 Index);

	bool SendBytes(FClient* Client, const uint8* Data, int32 NumBytes);
	void SendTextFrame(FClient* Client, const FString& Message);
	void SendControlFrame(FClient* Client, uint8 Opcode, const TArray<uint8>& Payload);
	void MarkClientForRemoval(FClient* Client);

	static FString JsonEscape(const FString& Value);
	bool TryReadFrame(FClient* Client, uint8& OutOpcode, bool& bOutFinal, TArray<uint8>& OutPayload);
};
