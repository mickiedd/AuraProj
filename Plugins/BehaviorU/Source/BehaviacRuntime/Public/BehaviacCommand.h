// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

// BehaviacTypes.h 已包含 CoreMinimal.h，且 clangd 已为其建立索引
#include "BehaviacTypes.h"

/**
 * 行为树命令类型。
 * 纯 C++ 枚举，不暴露给 Blueprint（内部实现细节）。
 * 后续扩展时在此添加新类型。
 */
enum class EBehaviacCommandType : uint8
{
	/** 调用 Agent 上的方法（Action 节点使用） */
	ExecuteMethod,
};

/**
 * 行为树命令消息。
 *
 * 由 Action 节点在树 Tick 期间生产，压入 UBehaviacAgentComponent 的命令队列。
 * 下一帧 TickComponent 开头由 ProcessCommandQueue() 取出并执行。
 *
 * bNeedResult = true  → 执行后将结果写入 CommandResults 表，Action 节点下一帧读取。
 * bNeedResult = false → 即发即忘，方法仍会执行（保留副作用），但结果被忽略。
 */
struct FBehaviacCommand
{
	/** 命令类型 */
	EBehaviacCommandType Type = EBehaviacCommandType::ExecuteMethod;

	/** ExecuteMethod 时要调用的方法名 */
	FString MethodName;

	/** 唯一命令 ID，由 EnqueueMethodCommand() 分配 */
	uint64 CommandId = 0;

	/** 是否需要把执行结果写回结果表供 Action 节点读取 */
	bool bNeedResult = true;
};
