// Copyright Druid Mechanics

#include "UI/WebUI/WebUIWebSocketServer.h"

#include "IPAddress.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "SocketSubsystem.h"

namespace WebUIWebSocketPrivate
{
	constexpr uint8 CloseOpcode = 0x8;
	constexpr uint8 PingOpcode = 0x9;
	constexpr uint8 PongOpcode = 0xA;
	constexpr int32 MaxMessageBytes = 4 * 1024 * 1024;

	bool SendAll(FSocket* Socket, const uint8* Data, int32 NumBytes)
	{
		if (!Socket || !Data || NumBytes <= 0)
		{
			return true;
		}

		int32 Offset = 0;
		while (Offset < NumBytes)
		{
			int32 Sent = 0;
			if (!Socket->Send(Data + Offset, NumBytes - Offset, Sent) || Sent <= 0)
			{
				return false;
			}
			Offset += Sent;
		}
		return true;
	}

	uint64 ReadBigEndian(const uint8* Bytes, int32 Count)
	{
		uint64 Value = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Value = (Value << 8) | Bytes[Index];
		}
		return Value;
	}
}

FWebUIWebSocketServer::FWebUIWebSocketServer() = default;

FWebUIWebSocketServer::~FWebUIWebSocketServer()
{
	Stop();
}

bool FWebUIWebSocketServer::Start(uint32 Port)
{
	if (bRunning)
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	ListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("AuraWebUIListen"), false);
	if (!ListenSocket)
	{
		return false;
	}

	ListenSocket->SetNonBlocking(true);
	ListenSocket->SetReuseAddr(true);

	TSharedRef<FInternetAddr> BindAddress = SocketSubsystem->CreateInternetAddr();
	bool bValidIp = false;
	BindAddress->SetIp(TEXT("127.0.0.1"), bValidIp); // local UI only
	if (!bValidIp)
	{
		SocketSubsystem->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}
	BindAddress->SetPort(static_cast<int32>(Port));

	if (!ListenSocket->Bind(*BindAddress) || !ListenSocket->Listen(8))
	{
		SocketSubsystem->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}

	ListenPort = Port;
	bRunning = true;
	return true;
}

void FWebUIWebSocketServer::Stop()
{
	if (!bRunning && !ListenSocket)
	{
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (FClient* Client : Clients)
	{
		if (Client && Client->Socket)
		{
			Client->Socket->Close();
			if (SocketSubsystem)
			{
				SocketSubsystem->DestroySocket(Client->Socket);
			}
			Client->Socket = nullptr;
		}
		delete Client;
	}
	Clients.Reset();

	if (ListenSocket)
	{
		ListenSocket->Close();
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(ListenSocket);
		}
		ListenSocket = nullptr;
	}

	bRunning = false;
	ListenPort = 0;
	if (ConnectionHandler)
	{
		ConnectionHandler(false);
	}
}

void FWebUIWebSocketServer::Tick()
{
	if (!bRunning)
	{
		return;
	}

	AcceptPending();
	for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
	{
		if (Clients[Index] && Clients[Index]->Socket)
		{
			ReceiveFrom(Clients[Index]);
		}
		if (!Clients[Index] || !Clients[Index]->Socket)
		{
			RemoveClient(Index);
		}
	}
}

void FWebUIWebSocketServer::AcceptPending()
{
	if (!ListenSocket)
	{
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return;
	}

	bool bHasPending = false;
	while (ListenSocket->HasPendingConnection(bHasPending) && bHasPending)
	{
		TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
		FSocket* NewSocket = ListenSocket->Accept(*RemoteAddress, TEXT("AuraWebUIClient"));
		if (!NewSocket)
		{
			break;
		}

		NewSocket->SetNonBlocking(true);
		FClient* Client = new FClient();
		Client->Socket = NewSocket;
		Clients.Add(Client);
	}
}

void FWebUIWebSocketServer::ReceiveFrom(FClient* Client)
{
	if (!Client || !Client->Socket)
	{
		return;
	}

	uint32 PendingBytes = 0;
	while (Client->Socket->HasPendingData(PendingBytes) && PendingBytes > 0)
	{
		const int32 OldSize = Client->ReceiveBuffer.Num();
		if (OldSize >= WebUIWebSocketPrivate::MaxMessageBytes)
		{
			MarkClientForRemoval(Client);
			return;
		}
		const int32 ReadSize = FMath::Min<int32>(static_cast<int32>(PendingBytes), WebUIWebSocketPrivate::MaxMessageBytes - OldSize);
		Client->ReceiveBuffer.SetNumUninitialized(OldSize + ReadSize);

		int32 BytesRead = 0;
		if (!Client->Socket->Recv(Client->ReceiveBuffer.GetData() + OldSize, ReadSize, BytesRead) || BytesRead <= 0)
		{
			MarkClientForRemoval(Client);
			return;
		}
		Client->ReceiveBuffer.SetNum(OldSize + BytesRead);
	}

	if (Client->ReceiveBuffer.IsEmpty())
	{
		return;
	}

	if (Client->State == EClientState::Handshaking)
	{
		HandleHandshake(Client);
	}
	else
	{
		HandleFrames(Client);
	}
}

void FWebUIWebSocketServer::HandleHandshake(FClient* Client)
{
	int32 HeaderEnd = INDEX_NONE;
	for (int32 Index = 0; Index + 3 < Client->ReceiveBuffer.Num(); ++Index)
	{
		if (Client->ReceiveBuffer[Index] == '\r'
			&& Client->ReceiveBuffer[Index + 1] == '\n'
			&& Client->ReceiveBuffer[Index + 2] == '\r'
			&& Client->ReceiveBuffer[Index + 3] == '\n')
		{
			HeaderEnd = Index;
			break;
		}
	}
	if (HeaderEnd == INDEX_NONE)
	{
		if (Client->ReceiveBuffer.Num() > WebUIWebSocketPrivate::MaxMessageBytes)
		{
			MarkClientForRemoval(Client);
		}
		return;
	}

	const int32 HeaderBytes = HeaderEnd + 4;
	TArray<uint8> HeaderData;
	HeaderData.Append(Client->ReceiveBuffer.GetData(), HeaderEnd);
	HeaderData.Add(0);
	Client->ReceiveBuffer.RemoveAt(0, HeaderBytes, EAllowShrinking::No);

	const FString Request = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(HeaderData.GetData())));
	FString ClientKey;
	TArray<FString> HeaderLines;
	Request.ParseIntoArrayLines(HeaderLines);
	for (const FString& Line : HeaderLines)
	{
		if (Line.StartsWith(TEXT("Sec-WebSocket-Key:"), ESearchCase::IgnoreCase))
		{
			ClientKey = Line.Mid(FCString::Strlen(TEXT("Sec-WebSocket-Key:"))).TrimStartAndEnd();
			break;
		}
	}

	if (ClientKey.IsEmpty())
	{
		MarkClientForRemoval(Client);
		return;
	}

	const FString MagicGuid = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	const FString Combined = ClientKey + MagicGuid;
	FTCHARToUTF8 CombinedUtf8(*Combined);
	uint8 Digest[20];
	FSHA1 Sha1;
	Sha1.Update(reinterpret_cast<const uint8*>(CombinedUtf8.Get()), CombinedUtf8.Length());
	Sha1.Final();
	Sha1.GetHash(Digest);

	const FString AcceptKey = FBase64::Encode(Digest, UE_ARRAY_COUNT(Digest));
	const FString Response = FString::Printf(
		TEXT("HTTP/1.1 101 Switching Protocols\r\n")
		TEXT("Upgrade: websocket\r\n")
		TEXT("Connection: Upgrade\r\n")
		TEXT("Sec-WebSocket-Accept: %s\r\n")
		TEXT("\r\n"),
		*AcceptKey);
	FTCHARToUTF8 ResponseUtf8(*Response);
	if (!SendBytes(Client, reinterpret_cast<const uint8*>(ResponseUtf8.Get()), ResponseUtf8.Length()))
	{
		return;
	}

	Client->State = EClientState::Connected;
	SendTextFrame(Client, TEXT("{\"type\":\"hello\",\"protocol\":1}"));
	if (ConnectionHandler && Clients.Num() == 1)
	{
		ConnectionHandler(true);
	}
}

void FWebUIWebSocketServer::HandleFrames(FClient* Client)
{
	while (Client && Client->Socket && !Client->ReceiveBuffer.IsEmpty())
	{
		uint8 Opcode = 0;
		bool bFinal = false;
		TArray<uint8> Payload;
		if (!TryReadFrame(Client, Opcode, bFinal, Payload))
		{
			return;
		}

		if (!bFinal)
		{
			MarkClientForRemoval(Client);
			return;
		}

		switch (Opcode)
		{
		case 0x1: // text
			Payload.Add(0);
			if (MessageHandler)
			{
				MessageHandler(FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Payload.GetData()))));
			}
			break;
		case WebUIWebSocketPrivate::PingOpcode:
			SendControlFrame(Client, WebUIWebSocketPrivate::PongOpcode, Payload);
			break;
		case WebUIWebSocketPrivate::CloseOpcode:
			SendControlFrame(Client, WebUIWebSocketPrivate::CloseOpcode, Payload);
			MarkClientForRemoval(Client);
			return;
		default:
			break;
		}
	}
}

bool FWebUIWebSocketServer::TryReadFrame(FClient* Client, uint8& OutOpcode, bool& bOutFinal, TArray<uint8>& OutPayload)
{
	if (!Client || Client->ReceiveBuffer.Num() < 2)
	{
		return false;
	}

	const uint8* Data = Client->ReceiveBuffer.GetData();
	bOutFinal = (Data[0] & 0x80) != 0;
	OutOpcode = Data[0] & 0x0F;
	const bool bMasked = (Data[1] & 0x80) != 0;
	uint64 PayloadLength = Data[1] & 0x7F;
	int32 HeaderLength = 2;

	if (PayloadLength == 126)
	{
		if (Client->ReceiveBuffer.Num() < 4) return false;
		PayloadLength = WebUIWebSocketPrivate::ReadBigEndian(Data + 2, 2);
		HeaderLength += 2;
	}
	else if (PayloadLength == 127)
	{
		if (Client->ReceiveBuffer.Num() < 10) return false;
		PayloadLength = WebUIWebSocketPrivate::ReadBigEndian(Data + 2, 8);
		HeaderLength += 8;
	}

	if (PayloadLength > WebUIWebSocketPrivate::MaxMessageBytes)
	{
		MarkClientForRemoval(Client);
		return false;
	}

	if (!bMasked)
	{
		MarkClientForRemoval(Client);
		return false;
	}

	if (bMasked)
	{
		HeaderLength += 4;
	}

	const uint64 FrameLength = static_cast<uint64>(HeaderLength) + PayloadLength;
	if (FrameLength > static_cast<uint64>(Client->ReceiveBuffer.Num()))
	{
		return false;
	}

	const uint8* Mask = bMasked ? Data + HeaderLength - 4 : nullptr;
	OutPayload.SetNumUninitialized(static_cast<int32>(PayloadLength));
	for (int32 Index = 0; Index < OutPayload.Num(); ++Index)
	{
		const uint8 SourceByte = Data[HeaderLength + Index];
		OutPayload[Index] = bMasked ? (SourceByte ^ Mask[Index % 4]) : SourceByte;
	}

	Client->ReceiveBuffer.RemoveAt(0, static_cast<int32>(FrameLength), EAllowShrinking::No);
	return true;
}

void FWebUIWebSocketServer::RemoveClient(int32 Index)
{
	if (!Clients.IsValidIndex(Index))
	{
		return;
	}

	FClient* Client = Clients[Index];
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Client && Client->Socket)
	{
		Client->Socket->Close();
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(Client->Socket);
		}
	}
	delete Client;
	Clients.RemoveAtSwap(Index);

	if (ConnectionHandler && Clients.IsEmpty())
	{
		ConnectionHandler(false);
	}
}

bool FWebUIWebSocketServer::SendBytes(FClient* Client, const uint8* Data, int32 NumBytes)
{
	if (!Client || !Client->Socket)
	{
		return false;
	}
	if (!WebUIWebSocketPrivate::SendAll(Client->Socket, Data, NumBytes))
	{
		MarkClientForRemoval(Client);
		return false;
	}
	return true;
}

void FWebUIWebSocketServer::SendTextFrame(FClient* Client, const FString& Message)
{
	if (!Client || Client->State != EClientState::Connected)
	{
		return;
	}

	FTCHARToUTF8 Utf8(*Message);
	const uint64 PayloadLength = static_cast<uint64>(Utf8.Length());
	TArray<uint8> Frame;
	Frame.Reserve(static_cast<int32>(PayloadLength) + 10);
	Frame.Add(0x81);
	if (PayloadLength <= 125)
	{
		Frame.Add(static_cast<uint8>(PayloadLength));
	}
	else if (PayloadLength <= 0xFFFF)
	{
		Frame.Add(126);
		Frame.Add(static_cast<uint8>((PayloadLength >> 8) & 0xFF));
		Frame.Add(static_cast<uint8>(PayloadLength & 0xFF));
	}
	else
	{
		Frame.Add(127);
		for (int32 Shift = 56; Shift >= 0; Shift -= 8)
		{
			Frame.Add(static_cast<uint8>((PayloadLength >> Shift) & 0xFF));
		}
	}
	Frame.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	SendBytes(Client, Frame.GetData(), Frame.Num());
}

void FWebUIWebSocketServer::SendControlFrame(FClient* Client, uint8 Opcode, const TArray<uint8>& Payload)
{
	if (!Client || !Client->Socket || Payload.Num() > 125)
	{
		return;
	}

	TArray<uint8> Frame;
	Frame.Add(static_cast<uint8>(0x80 | (Opcode & 0x0F)));
	Frame.Add(static_cast<uint8>(Payload.Num()));
	Frame.Append(Payload);
	SendBytes(Client, Frame.GetData(), Frame.Num());
}

void FWebUIWebSocketServer::BroadcastText(const FString& Message)
{
	for (FClient* Client : Clients)
	{
		SendTextFrame(Client, Message);
	}
}

void FWebUIWebSocketServer::MarkClientForRemoval(FClient* Client)
{
	if (!Client || !Client->Socket)
	{
		return;
	}
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Client->Socket->Close();
	if (SocketSubsystem)
	{
		SocketSubsystem->DestroySocket(Client->Socket);
	}
	Client->Socket = nullptr;
}

FString FWebUIWebSocketServer::JsonEscape(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	return Escaped;
}
