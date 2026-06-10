// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

class UBehaviacAgentComponent;
struct FBehaviacBlackboardSnapshot;

/**
 * FBehaviacDebugServer — 行为树运行时黑板调试 WebSocket 服务器
 *
 * 基于 UE 内置 FSocket (BSD sockets) 实现，零外部模块依赖。
 * 支持 RFC 6455 WebSocket 协议（握手 + text frame）。
 *
 * 控制台命令：
 *   Behaviac.Debug.StartServer [port]      默认 17654
 *   Behaviac.Debug.StopServer
 *   Behaviac.Debug.BroadcastInterval N     0=每帧
 *
 * 协议（UTF-8 JSON）：
 *   Game→Editor: bb_snapshot | agent_removed | agent_tree_source
 *   Editor→Game: request_snapshot
 */
class BEHAVIACRUNTIME_API FBehaviacDebugServer
{
public:
	FBehaviacDebugServer();
	~FBehaviacDebugServer();

	FBehaviacDebugServer(const FBehaviacDebugServer&)            = delete;
	FBehaviacDebugServer& operator=(const FBehaviacDebugServer&) = delete;

	// ── 生命周期 ──────────────────────────────────────────────────────────────

	bool Start(uint32 Port = 17654);
	void Stop();
	bool IsRunning() const { return bRunning; }
	uint32 GetPort() const { return ListenPort; }

	// ── 帧驱动（游戏线程）────────────────────────────────────────────────────

	/** 接受新连接、接收客户端消息。每帧调用一次。*/
	void Tick();

	// ── 数据推送 ──────────────────────────────────────────────────────────────

	void BroadcastSnapshots(const TArray<UBehaviacAgentComponent*>& Agents);
	void SendAgentRemoved(uint64 AgentPtrId);

	/**
	 * 向所有尚未同步过树路径的客户端推送当前所有 Agent 的行为树 XML 源文件路径。
	 * 每个客户端仅推送一次（握手完成后首帧），后续帧空操作。
	 * 必须在游戏线程调用，通常在 BroadcastSnapshots 之前调用。
	 */
	void BroadcastTreeSourcePaths(const TArray<UBehaviacAgentComponent*>& Agents);

	int32 GetClientCount() const { return Clients.Num(); }
	int32 BroadcastIntervalFrames = 0;

private:
	// ── 客户端状态 ────────────────────────────────────────────────────────────

	enum class EClientState : uint8
	{
		Handshaking,    // 正在接收 HTTP Upgrade 请求
		Connected,      // WebSocket 握手已完成
	};

	struct FClientConn
	{
		FSocket*      Socket       = nullptr;
		EClientState  State        = EClientState::Handshaking;
		TArray<uint8> RecvBuf;      // 未处理的接收缓冲
		/** 是否已向该客户端推送过行为树 XML 源路径；首次连接后置 false，推送完成后置 true */
		bool          bTreePathsSynced = false;
	};

	// ── 套接字 ────────────────────────────────────────────────────────────────

	FSocket*              ListenSocket = nullptr;
	TArray<FClientConn*>  Clients;

	bool   bRunning   = false;
	uint32 ListenPort = 17654;
	int32  FrameCount = 0;
	bool   bImmediateSnapshotRequested = false;

	struct FAgentActivityState
	{
		TArray<int32>   ActiveNodeIds;
		TArray<FString> ActiveNodeStatuses;
	};

	TMap<uint64, FAgentActivityState> AgentActivityStates;

	// ── 内部处理 ──────────────────────────────────────────────────────────────

	/** 接受所有等待中的新连接（非阻塞） */
	void AcceptPending();

	/** 从单个客户端读取所有可用数据 */
	void ReceiveFrom(FClientConn* Conn);

	/** 处理 HTTP Upgrade 握手请求 */
	void HandleHandshake(FClientConn* Conn);

	/** 处理已握手客户端的 WebSocket frame */
	void HandleFrames(FClientConn* Conn);

	/** 关闭并移除一个客户端 */
	void RemoveClient(int32 Index);

	// ── 发送工具 ──────────────────────────────────────────────────────────────

	/** 向单个客户端发送 WebSocket text frame */
	void SendTextFrame(FClientConn* Conn, const FString& JsonStr);

	/** 向所有已 Connected 客户端广播 */
	void BroadcastJson(const FString& JsonStr);

	// ── JSON 序列化 ───────────────────────────────────────────────────────────

	static FString BuildSnapshotJson(const FBehaviacBlackboardSnapshot& Snap);
	static FString BuildRemovedJson(uint64 AgentPtrId);
	static FString BuildTreeSourcePathJson(uint64 AgentPtrId, const FString& AgentName, const FString& TreeName, const FString& SourcePath);
	static FString JsonEscape(const FString& Str);
};
