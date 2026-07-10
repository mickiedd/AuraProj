// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUDebugServer.h"
#include "BehaviorUAgent.h"
#include "BehaviorUTypes.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"

// ============================================================================
// 构造 / 析构
// ============================================================================

FBehaviorUDebugServer::FBehaviorUDebugServer()  = default;

FBehaviorUDebugServer::~FBehaviorUDebugServer()
{
	Stop();
}

// ============================================================================
// 生命周期
// ============================================================================

bool FBehaviorUDebugServer::Start(uint32 Port)
{
	if (bRunning)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorUDebug] 服务器已在运行（端口 %u）。"), ListenPort);
		return false;
	}

	ISocketSubsystem* SocketSS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSS)
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorUDebug] 无法获取 SocketSubsystem。"));
		return false;
	}

	// 创建 TCP 监听套接字
	ListenSocket = SocketSS->CreateSocket(NAME_Stream, TEXT("BehaviorUDebugListen"), false);
	if (!ListenSocket)
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorUDebug] CreateSocket 失败。"));
		return false;
	}

	// 设置非阻塞 + 地址复用
	ListenSocket->SetNonBlocking(true);
	ListenSocket->SetReuseAddr(true);

	// 绑定到 0.0.0.0:Port
	TSharedRef<FInternetAddr> BindAddr = SocketSS->CreateInternetAddr();
	BindAddr->SetIp(0);             // INADDR_ANY
	BindAddr->SetPort(static_cast<int32>(Port));

	if (!ListenSocket->Bind(*BindAddr))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorUDebug] Bind 失败（端口 %u 可能已占用）。"), Port);
		SocketSS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}

	if (!ListenSocket->Listen(8))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorUDebug] Listen 失败。"));
		SocketSS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}

	ListenPort  = Port;
	bRunning    = true;
	FrameCount  = 0;
	bImmediateSnapshotRequested = false;

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug] WebSocket 调试服务器已启动，监听端口 %u"), Port);
	return true;
}

void FBehaviorUDebugServer::Stop()
{
	if (!bRunning)
	{
		return;
	}

	// 通知所有已连接客户端（尽力而为）
	BroadcastJson(TEXT("{\"type\":\"server_stopping\"}"));

	// 关闭所有客户端连接
	ISocketSubsystem* SocketSS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (FClientConn* Conn : Clients)
	{
		if (Conn && Conn->Socket)
		{
			Conn->Socket->Close();
			if (SocketSS) { SocketSS->DestroySocket(Conn->Socket); }
		}
		delete Conn;
	}
	Clients.Reset();

	// 关闭监听套接字
	if (ListenSocket)
	{
		ListenSocket->Close();
		if (SocketSS) { SocketSS->DestroySocket(ListenSocket); }
		ListenSocket = nullptr;
	}

	bRunning = false;
	AgentActivityStates.Reset();
	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug] WebSocket 调试服务器已停止。"));
}

// ============================================================================
// 帧驱动
// ============================================================================

void FBehaviorUDebugServer::Tick()
{
	if (!bRunning) { return; }

	// 接受新连接
	AcceptPending();

	// 从每个客户端读取数据
	for (int32 i = Clients.Num() - 1; i >= 0; --i)
	{
		ReceiveFrom(Clients[i]);
		// ReceiveFrom 内部若检测到断开会调用 RemoveClient，这里检测指针是否仍在数组
		if (i < Clients.Num() && Clients[i] && !Clients[i]->Socket)
		{
			RemoveClient(i);
		}
	}
}

// ============================================================================
// 数据推送
// ============================================================================

void FBehaviorUDebugServer::BroadcastSnapshots(const TArray<UBehaviorUAgentComponent*>& Agents)
{
	if (!bRunning || Clients.IsEmpty()) { return; }

	++FrameCount;
	const bool bShouldBroadcast = bImmediateSnapshotRequested
		|| (BroadcastIntervalFrames <= 0)
		|| (FrameCount >= BroadcastIntervalFrames);

	if (!bShouldBroadcast) { return; }

	FrameCount = 0;
	bImmediateSnapshotRequested = false;

	struct FPendingSnapshotBroadcast
	{
		FBehaviorUBlackboardSnapshot Snapshot;
		int32 SwitchScore = 0;
		int32 OriginalIndex = 0;
	};

	auto CalculateNodeSwitchScore = [](const FBehaviorUBlackboardSnapshot& Snapshot,
		const FAgentActivityState* PreviousState,
		FAgentActivityState& OutNextState) -> int32
	{
		OutNextState.ActiveNodeIds.Reserve(Snapshot.ActiveNodes.Num());
		OutNextState.ActiveNodeStatuses.Reserve(Snapshot.ActiveNodes.Num());

		for (const FBehaviorUNodeSnapshot& Node : Snapshot.ActiveNodes)
		{
			OutNextState.ActiveNodeIds.Add(Node.NodeId);
			OutNextState.ActiveNodeStatuses.Add(Node.Status);
		}

		if (!PreviousState)
		{
			return Snapshot.ActiveNodes.Num();
		}

		const int32 SharedCount = FMath::Min(
			PreviousState->ActiveNodeIds.Num(),
			OutNextState.ActiveNodeIds.Num());

		int32 SwitchScore = FMath::Abs(
			PreviousState->ActiveNodeIds.Num() - OutNextState.ActiveNodeIds.Num()) * 2;

		for (int32 NodeIndex = 0; NodeIndex < SharedCount; ++NodeIndex)
		{
			if (PreviousState->ActiveNodeIds[NodeIndex] != OutNextState.ActiveNodeIds[NodeIndex])
			{
				SwitchScore += 2;
				continue;
			}

			if (PreviousState->ActiveNodeStatuses[NodeIndex] != OutNextState.ActiveNodeStatuses[NodeIndex])
			{
				++SwitchScore;
			}
		}

		return SwitchScore;
	};

	TArray<FPendingSnapshotBroadcast> PendingBroadcasts;
	PendingBroadcasts.Reserve(Agents.Num());

	TSet<uint64> LiveAgentIds;
	LiveAgentIds.Reserve(Agents.Num());

	for (int32 AgentIndex = 0; AgentIndex < Agents.Num(); ++AgentIndex)
	{
		const UBehaviorUAgentComponent* Agent = Agents[AgentIndex];
		if (!Agent) { continue; }

		const uint64 AgentId = static_cast<uint64>(reinterpret_cast<uintptr_t>(Agent));
		LiveAgentIds.Add(AgentId);

		FPendingSnapshotBroadcast& Pending = PendingBroadcasts.AddDefaulted_GetRef();
		Pending.OriginalIndex = AgentIndex;
		Pending.Snapshot = Agent->GetBlackboardSnapshot();

		FAgentActivityState NextState;
		Pending.SwitchScore = CalculateNodeSwitchScore(Pending.Snapshot, AgentActivityStates.Find(AgentId), NextState);
		AgentActivityStates.Add(AgentId, MoveTemp(NextState));
	}

	for (auto It = AgentActivityStates.CreateIterator(); It; ++It)
	{
		if (!LiveAgentIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	PendingBroadcasts.Sort([](const FPendingSnapshotBroadcast& Lhs, const FPendingSnapshotBroadcast& Rhs)
	{
		if (Lhs.SwitchScore != Rhs.SwitchScore)
		{
			return Lhs.SwitchScore > Rhs.SwitchScore;
		}

		return Lhs.OriginalIndex < Rhs.OriginalIndex;
	});

	for (const FPendingSnapshotBroadcast& Pending : PendingBroadcasts)
	{
		BroadcastJson(BuildSnapshotJson(Pending.Snapshot));
	}
}

void FBehaviorUDebugServer::SendAgentRemoved(uint64 AgentPtrId)
{
	AgentActivityStates.Remove(AgentPtrId);
	if (!bRunning || Clients.IsEmpty()) { return; }
	BroadcastJson(BuildRemovedJson(AgentPtrId));
}

// ============================================================================
// 接受新连接
// ============================================================================

void FBehaviorUDebugServer::AcceptPending()
{
	if (!ListenSocket) { return; }

	bool bHasPending = false;
	while (ListenSocket->HasPendingConnection(bHasPending) && bHasPending)
	{
		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		FSocket* NewSock = ListenSocket->Accept(*RemoteAddr, TEXT("BehaviorUDebugClient"));
		if (!NewSock) { break; }

		NewSock->SetNonBlocking(true);

		FClientConn* Conn = new FClientConn();
		Conn->Socket = NewSock;
		Conn->State  = EClientState::Handshaking;
		Clients.Add(Conn);

		UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug] 客户端已连入，当前连接数: %d"), Clients.Num());
	}
}

// ============================================================================
// 接收数据
// ============================================================================

void FBehaviorUDebugServer::ReceiveFrom(FClientConn* Conn)
{
	if (!Conn || !Conn->Socket) { return; }

	uint32 PendingSize = 0;
	while (Conn->Socket->HasPendingData(PendingSize) && PendingSize > 0)
	{
		const int32 OldLen = Conn->RecvBuf.Num();
		Conn->RecvBuf.SetNum(OldLen + static_cast<int32>(PendingSize));

		int32 BytesRead = 0;
		const bool bOk = Conn->Socket->Recv(
			Conn->RecvBuf.GetData() + OldLen,
			static_cast<int32>(PendingSize),
			BytesRead);

		if (!bOk || BytesRead <= 0)
		{
			// 连接断开或错误
			Conn->RecvBuf.SetNum(OldLen);
			// 标记为待移除：将 Socket 置 null，Tick 中统一回收
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Conn->Socket);
			Conn->Socket = nullptr;
			return;
		}

		Conn->RecvBuf.SetNum(OldLen + BytesRead);
	}

	if (Conn->RecvBuf.IsEmpty()) { return; }

	if (Conn->State == EClientState::Handshaking)
	{
		HandleHandshake(Conn);
	}
	else
	{
		HandleFrames(Conn);
	}
}

// ============================================================================
// WebSocket 握手（RFC 6455）
// ============================================================================

void FBehaviorUDebugServer::HandleHandshake(FClientConn* Conn)
{
	// 等到收到完整的 HTTP 请求头（以 \r\n\r\n 结尾）
	// 追加 '\0' 保证 UTF8_TO_TCHAR 不越界读取，解析完成后立即 Reset
	Conn->RecvBuf.Add(0);
	const FString RawHttp = FString(UTF8_TO_TCHAR(
		reinterpret_cast<const ANSICHAR*>(Conn->RecvBuf.GetData())));

	if (!RawHttp.Contains(TEXT("\r\n\r\n")))
	{
		Conn->RecvBuf.Pop(false); // 移除临时 '\0'，等待更多数据
		return;
	}

	Conn->RecvBuf.Reset();

	// 提取 Sec-WebSocket-Key
	static const FString KeyHeader = TEXT("Sec-WebSocket-Key: ");
	int32 KeyStart = RawHttp.Find(KeyHeader, ESearchCase::IgnoreCase);
	if (KeyStart == INDEX_NONE)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorUDebug] 握手请求缺少 Sec-WebSocket-Key，拒绝连接。"));
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Conn->Socket);
		Conn->Socket = nullptr;
		return;
	}

	KeyStart += KeyHeader.Len();
	int32 KeyEnd = RawHttp.Find(TEXT("\r\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, KeyStart);
	const FString ClientKey = RawHttp.Mid(KeyStart, KeyEnd - KeyStart).TrimStartAndEnd();

	// 计算 Sec-WebSocket-Accept = Base64(SHA1(Key + GUID))
	const FString MagicGuid = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	const FString Combined  = ClientKey + MagicGuid;

	// SHA1
	uint8 Digest[20];
	FSHA1 Sha1;
	Sha1.Update(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*Combined)), FCString::Strlen(*Combined));
	Sha1.Final();
	Sha1.GetHash(Digest);

	const FString AcceptKey = FBase64::Encode(Digest, 20);

	// 发送 101 Switching Protocols
	const FString Response = FString::Printf(
		TEXT("HTTP/1.1 101 Switching Protocols\r\n")
		TEXT("Upgrade: websocket\r\n")
		TEXT("Connection: Upgrade\r\n")
		TEXT("Sec-WebSocket-Accept: %s\r\n")
		TEXT("\r\n"),
		*AcceptKey);

	FTCHARToUTF8 Utf8(*Response);
	int32 Sent = 0;
	Conn->Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent);

	Conn->State = EClientState::Connected;

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug] WebSocket 握手完成，客户端就绪。"));

	// 握手成功后立即请求一次全量快照
	bImmediateSnapshotRequested = true;
}

// ============================================================================
// WebSocket frame 解析（仅处理 text frame，客户端到服务器必须 mask）
// ============================================================================

void FBehaviorUDebugServer::HandleFrames(FClientConn* Conn)
{
	TArray<uint8>& Buf = Conn->RecvBuf;

	while (Buf.Num() >= 2)
	{
		const uint8 Byte0 = Buf[0];
		const uint8 Byte1 = Buf[1];

		const uint8  Opcode    = Byte0 & 0x0F;
		const bool   bMasked   = (Byte1 & 0x80) != 0;
		uint64       PayloadLen = Byte1 & 0x7F;
		int32        HeaderSize = 2;

		// 扩展长度
		if (PayloadLen == 126)
		{
			if (Buf.Num() < 4) { return; } // 等待更多数据
			PayloadLen = (static_cast<uint64>(Buf[2]) << 8) | Buf[3];
			HeaderSize = 4;
		}
		else if (PayloadLen == 127)
		{
			if (Buf.Num() < 10) { return; }
			PayloadLen = 0;
			for (int32 k = 0; k < 8; ++k)
			{
				PayloadLen = (PayloadLen << 8) | Buf[2 + k];
			}
			HeaderSize = 10;
		}

		const int32 MaskSize   = bMasked ? 4 : 0;
		const int64 TotalFrame = HeaderSize + MaskSize + static_cast<int64>(PayloadLen);

		if (static_cast<int64>(Buf.Num()) < TotalFrame) { return; } // 帧还不完整

		// 解 mask
		TArray<uint8> Payload;
		Payload.SetNum(static_cast<int32>(PayloadLen));
		const uint8* SrcData = Buf.GetData() + HeaderSize + MaskSize;
		if (bMasked)
		{
			const uint8* Mask = Buf.GetData() + HeaderSize;
			for (int32 i = 0; i < static_cast<int32>(PayloadLen); ++i)
			{
				Payload[i] = SrcData[i] ^ Mask[i % 4];
			}
		}
		else
		{
			FMemory::Memcpy(Payload.GetData(), SrcData, static_cast<int32>(PayloadLen));
		}

		// 消费已处理的帧
		Buf.RemoveAt(0, static_cast<int32>(TotalFrame));

		// Opcode 8 = close，关闭连接
		if (Opcode == 8)
		{
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Conn->Socket);
			Conn->Socket = nullptr;
			return;
		}

		// Opcode 1 = text frame
		if (Opcode == 1 && Payload.Num() > 0)
		{
			Payload.Add(0); // null-terminate
			const FString JsonStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Payload.GetData())));
			if (JsonStr.Contains(TEXT("request_snapshot")))
			{
				UE_LOG(LogBehaviorU, Verbose, TEXT("[BehaviorUDebug] 收到 request_snapshot。"));
				bImmediateSnapshotRequested = true;
			}
		}
	}
}

// ============================================================================
// 客户端移除
// ============================================================================

void FBehaviorUDebugServer::RemoveClient(int32 Index)
{
	if (!Clients.IsValidIndex(Index)) { return; }
	FClientConn* Conn = Clients[Index];
	if (Conn)
	{
		if (Conn->Socket)
		{
			Conn->Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Conn->Socket);
		}
		delete Conn;
	}
	Clients.RemoveAtSwap(Index);
	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug] 客户端已断开，剩余连接数: %d"), Clients.Num());
}

// ============================================================================
// WebSocket text frame 发送
// ============================================================================

void FBehaviorUDebugServer::SendTextFrame(FClientConn* Conn, const FString& JsonStr)
{
	if (!Conn || !Conn->Socket || Conn->State != EClientState::Connected) { return; }

	FTCHARToUTF8 Utf8(*JsonStr);
	const int32  PayloadLen = Utf8.Length();
	const uint8* Payload    = reinterpret_cast<const uint8*>(Utf8.Get());

	// 构造 frame 头部（服务端→客户端不需要 mask）
	TArray<uint8> Frame;
	Frame.Reserve(10 + PayloadLen);

	Frame.Add(0x81); // FIN=1, Opcode=1 (text)

	if (PayloadLen <= 125)
	{
		Frame.Add(static_cast<uint8>(PayloadLen));
	}
	else if (PayloadLen <= 65535)
	{
		Frame.Add(126);
		Frame.Add(static_cast<uint8>((PayloadLen >> 8) & 0xFF));
		Frame.Add(static_cast<uint8>(PayloadLen        & 0xFF));
	}
	else
	{
		Frame.Add(127);
		// 转为 uint64 后再移位，避免 int32 右移 >31 位产生的未定义行为
		const uint64 PL64 = static_cast<uint64>(PayloadLen);
		for (int32 k = 7; k >= 0; --k)
		{
			Frame.Add(static_cast<uint8>((PL64 >> (k * 8)) & 0xFF));
		}
	}

	Frame.Append(Payload, PayloadLen);

	int32 Sent = 0;
	Conn->Socket->Send(Frame.GetData(), Frame.Num(), Sent);
}

void FBehaviorUDebugServer::BroadcastJson(const FString& JsonStr)
{
	for (int32 i = Clients.Num() - 1; i >= 0; --i)
	{
		FClientConn* Conn = Clients[i];
		if (!Conn || !Conn->Socket)
		{
			RemoveClient(i);
			continue;
		}
		if (Conn->State == EClientState::Connected)
		{
			SendTextFrame(Conn, JsonStr);
		}
	}
}

// ============================================================================
// JSON 序列化
// ============================================================================

/* static */
FString FBehaviorUDebugServer::JsonEscape(const FString& Str)
{
	FString Out;
	Out.Reserve(Str.Len() + 8);
	for (const TCHAR Ch : Str)
	{
		switch (Ch)
		{
			case TEXT('"'):  Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n");  break;
			case TEXT('\r'): Out += TEXT("\\r");  break;
			case TEXT('\t'): Out += TEXT("\\t");  break;
			default:         Out += Ch;           break;
		}
	}
	return Out;
}

/* static */
FString FBehaviorUDebugServer::BuildSnapshotJson(const FBehaviorUBlackboardSnapshot& Snap)
{
	FString Json;
	Json.Reserve(128 + (Snap.Properties.Num() + Snap.ObjectPropertyNames.Num()) * 40);

	Json += TEXT("{\"type\":\"bb_snapshot\",");
	Json += TEXT("\"agent_id\":\"");    Json += Snap.AgentId;               Json += TEXT("\",");
	Json += TEXT("\"agent_name\":\"");  Json += JsonEscape(Snap.AgentName);  Json += TEXT("\",");
	Json += TEXT("\"tree_name\":\"");   Json += JsonEscape(Snap.TreeName);   Json += TEXT("\",");
	Json += TEXT("\"tree_status\":\""); Json += Snap.TreeStatus;             Json += TEXT("\",");

	Json += TEXT("\"properties\":{");
	bool bFirst = true;
	for (const TPair<FString, FString>& Pair : Snap.Properties)
	{
		if (!bFirst) { Json += TEXT(","); }
		Json += TEXT("\""); Json += JsonEscape(Pair.Key);   Json += TEXT("\":\"");
		Json +=             JsonEscape(Pair.Value);          Json += TEXT("\"");
		bFirst = false;
	}
	Json += TEXT("},\"object_properties\":{");
	bFirst = true;
	for (const TPair<FString, FString>& Pair : Snap.ObjectPropertyNames)
	{
		if (!bFirst) { Json += TEXT(","); }
		Json += TEXT("\""); Json += JsonEscape(Pair.Key);   Json += TEXT("\":\"");
		Json +=             JsonEscape(Pair.Value);          Json += TEXT("\"");
		bFirst = false;
	}

	// 活跃节点数组：按父节点先序排列，反映本帧执行路径
	Json += TEXT("},\"nodes\":[");
	bFirst = true;
	for (const FBehaviorUNodeSnapshot& N : Snap.ActiveNodes)
	{
		if (!bFirst) { Json += TEXT(","); }
		Json += TEXT("{\"id\":");
		Json += FString::FromInt(N.NodeId);
		Json += TEXT(",\"class\":\"");
		Json += JsonEscape(N.NodeClass);
		Json += TEXT("\",\"status\":\"");
		Json += N.Status;
		Json += TEXT("\"}");
		bFirst = false;
	}
	Json += TEXT("]}");
	return Json;
}

/* static */
FString FBehaviorUDebugServer::BuildRemovedJson(uint64 AgentPtrId)
{
	return FString::Printf(TEXT("{\"type\":\"agent_removed\",\"agent_id\":\"%llu\"}"), AgentPtrId);
}

/* static */
FString FBehaviorUDebugServer::BuildTreeSourcePathJson(uint64 AgentPtrId, const FString& AgentName, const FString& TreeName, const FString& SourcePath)
{
	FString Json;
	Json.Reserve(96 + AgentName.Len() + TreeName.Len() + SourcePath.Len());
	Json += TEXT("{\"type\":\"agent_tree_source\",\"agent_id\":\"");
	Json += FString::Printf(TEXT("%llu"), AgentPtrId);
	Json += TEXT("\",\"agent_name\":\"");
	Json += JsonEscape(AgentName);
	Json += TEXT("\",\"tree_name\":\"");
	Json += JsonEscape(TreeName);
	Json += TEXT("\",\"source_path\":\"");
	Json += JsonEscape(SourcePath);
	Json += TEXT("\"}");
	return Json;
}

// ============================================================================
// 行为树 XML 源路径首次同步
// ============================================================================

void FBehaviorUDebugServer::BroadcastTreeSourcePaths(const TArray<UBehaviorUAgentComponent*>& Agents)
{
	if (!bRunning || Clients.IsEmpty()) { return; }

	// 判断是否有尚未同步的客户端，若没有则直接跳过，避免无谓遍历
	bool bAnyUnsynced = false;
	for (const FClientConn* Conn : Clients)
	{
		if (Conn && Conn->State == EClientState::Connected && !Conn->bTreePathsSynced)
		{
			bAnyUnsynced = true;
			break;
		}
	}
	if (!bAnyUnsynced) { return; }

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug][TreeSync] 检测到未同步客户端，开始收集 Agent 行为树 XML 路径。当前 Agent 数: %d"), Agents.Num());

	struct FTreeSourcePathPayload
	{
		uint64 AgentId = 0;
		FString AgentName;
		FString TreeName;
		FString SourcePath;
	};
	TArray<FTreeSourcePathPayload> TreeSourcePaths;
	TreeSourcePaths.Reserve(Agents.Num());
	for (const UBehaviorUAgentComponent* Agent : Agents)
	{
		if (!Agent) { continue; }

		const FString TreeName = Agent->GetCurrentTreeName();
		const FString SourcePath = Agent->GetCurrentTreeSourceFilePath();
		const uint64 AgentId = static_cast<uint64>(reinterpret_cast<uintptr_t>(Agent));
		const FString AgentName = Agent->GetName();
		UE_LOG(LogBehaviorU, Log,
			TEXT("[BehaviorUDebug][TreeSync]   Agent=%s  TreeName='%s'  SourcePath='%s'"),
			*AgentName, *TreeName, *SourcePath);
		if (TreeName.IsEmpty() || SourcePath.IsEmpty()) { continue; }

		FTreeSourcePathPayload& Payload = TreeSourcePaths.AddDefaulted_GetRef();
		Payload.AgentId = AgentId;
		Payload.AgentName = AgentName;
		Payload.TreeName = TreeName;
		Payload.SourcePath = SourcePath;
	}

	if (TreeSourcePaths.IsEmpty())
	{
		UE_LOG(LogBehaviorU, Warning,
			TEXT("[BehaviorUDebug][TreeSync] 未收集到任何可用行为树 XML 路径（Agent 可能尚未加载树，或树未保留源文件路径）。"));
		return;
	}

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug][TreeSync] 收集到 %d 个 Agent 的行为树路径，准备推送。"), TreeSourcePaths.Num());

	// 仅向尚未同步的已连接客户端推送
	for (FClientConn* Conn : Clients)
	{
		if (!Conn || !Conn->Socket || Conn->State != EClientState::Connected || Conn->bTreePathsSynced)
		{
			continue;
		}

		for (const FTreeSourcePathPayload& Payload : TreeSourcePaths)
		{
			const FString Json = BuildTreeSourcePathJson(Payload.AgentId, Payload.AgentName, Payload.TreeName, Payload.SourcePath);
			UE_LOG(LogBehaviorU, Log,
				TEXT("[BehaviorUDebug][TreeSync] 推送 Agent=%s Tree='%s' Path='%s'，JSON 总字节数: %d"),
				*Payload.AgentName, *Payload.TreeName, *Payload.SourcePath, Json.Len());
			SendTextFrame(Conn, Json);
		}

		Conn->bTreePathsSynced = true;
		UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorUDebug][TreeSync] 已向客户端推送 %d 个 Agent 的行为树路径，标记同步完成。"), TreeSourcePaths.Num());
	}
}
