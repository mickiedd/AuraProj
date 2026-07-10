// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "Stats/Stats.h"

/**
 * BehaviorU 性能统计组
 *
 * 在运行时通过以下方式查看：
 *   stat BehaviorU              — 控制台命令
 *   Unreal Insights → Timing → BehaviorU
 *
 * 统计层级：
 *   子系统级（游戏线程）
 *     └─ Phase1: ProcessCommandQueues  [GT]
 *     └─ Phase2: Dispatch Workers      [GT → WT]
 *           └─ Phase2: Wait Workers Done  [GT stall]
 *   Agent 级（游戏线程）
 *     └─ ProcessCommandQueue  → ExecuteMethod
 *   Agent 级（工作线程）
 *     └─ TickBehaviorTree → Task: Execute
 *           └─ Task: CheckPreconditions
 *           └─ Task: ApplyEffectors
 */
DECLARE_STATS_GROUP(TEXT("BehaviorU"), STATGROUP_BehaviorU, STATCAT_Advanced);

// ─── 子系统级（游戏线程）────────────────────────────────────────────
// 整体 Tick 耗时，包含 Phase1 + Phase2 全部开销
DECLARE_CYCLE_STAT_EXTERN(TEXT("BehaviorU Tick"),
	STAT_BehaviorU_Tick,             STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// Phase1：游戏线程串行处理所有 Agent 命令队列（TS/Blueprint/C++ 回调）
DECLARE_CYCLE_STAT_EXTERN(TEXT("Phase1: ProcessCommandQueues"),
	STAT_BehaviorU_Phase1,           STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// Phase2：切片分发至工作线程池并等待全部完成
DECLARE_CYCLE_STAT_EXTERN(TEXT("Phase2: Dispatch + Wait Workers"),
	STAT_BehaviorU_Phase2,           STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// Phase2 中游戏线程阻塞等待工作线程返回的纯等待耗时（越小说明线程均衡越好）
DECLARE_CYCLE_STAT_EXTERN(TEXT("Phase2: Wait Workers Done"),
	STAT_BehaviorU_Phase2_Wait,      STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// ─── Agent 级（游戏线程）──────────────────────────────────────────────
// 单个 Agent 的命令队列处理（Phase1 内部，含所有方法调用）
DECLARE_CYCLE_STAT_EXTERN(TEXT("ProcessCommandQueue"),
	STAT_BehaviorU_ProcessCmdQueue,  STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// 单次 TS/Blueprint/C++ 方法调用（Phase1 内部热路径）
DECLARE_CYCLE_STAT_EXTERN(TEXT("ExecuteMethod"),
	STAT_BehaviorU_ExecuteMethod,    STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// ─── Agent 级（工作线程）──────────────────────────────────────────────
// 单个 Agent 的行为树 Tick（工作线程调用，含节点遍历全部开销）
DECLARE_CYCLE_STAT_EXTERN(TEXT("TickBehaviorTree"),
	STAT_BehaviorU_TickBT,           STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// ─── 工作线程总批次（工作线程）────────────────────────────────────────
// 单个工作线程在一帧内处理其分配切片的全部 Agent 的耗时
DECLARE_CYCLE_STAT_EXTERN(TEXT("WorkerThread: TickAgents Batch"),
	STAT_BehaviorU_WorkerTickAgents, STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// ─── 任务级（工作线程热路径）──────────────────────────────────────────
// 单节点任务执行（Enter → Update → Exit 全流程）
DECLARE_CYCLE_STAT_EXTERN(TEXT("Task: Execute"),
	STAT_BehaviorU_Task_Execute,     STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// 前置条件检查（每次 Enter/Update 前调用）
DECLARE_CYCLE_STAT_EXTERN(TEXT("Task: CheckPreconditions"),
	STAT_BehaviorU_Task_Precond,     STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// 效应器应用（节点完成时调用）
DECLARE_CYCLE_STAT_EXTERN(TEXT("Task: ApplyEffectors"),
	STAT_BehaviorU_Task_Effectors,   STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// ─── 数量统计（每帧快照）─────────────────────────────────────────────
// 子系统中已注册的 Agent 总数
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Registered Agents"),
	STAT_BehaviorU_RegisteredAgents, STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);

// 本帧参与 Tick 的有效 Agent 数（排除弱引用失效的 Agent）
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Active Agents"),
	STAT_BehaviorU_ActiveAgents,     STATGROUP_BehaviorU, BEHAVIORURUNTIME_API);
