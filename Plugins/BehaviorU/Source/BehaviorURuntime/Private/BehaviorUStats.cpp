// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUStats.h"

// ─── 子系统级
DEFINE_STAT(STAT_BehaviorU_Tick);
DEFINE_STAT(STAT_BehaviorU_Phase1);
DEFINE_STAT(STAT_BehaviorU_Phase2);
DEFINE_STAT(STAT_BehaviorU_Phase2_Wait);

// ─── Agent 级（游戏线程）
DEFINE_STAT(STAT_BehaviorU_ProcessCmdQueue);
DEFINE_STAT(STAT_BehaviorU_ExecuteMethod);

// ─── Agent 级（工作线程）
DEFINE_STAT(STAT_BehaviorU_TickBT);

// ─── 工作线程
DEFINE_STAT(STAT_BehaviorU_WorkerTickAgents);

// ─── 任务级
DEFINE_STAT(STAT_BehaviorU_Task_Execute);
DEFINE_STAT(STAT_BehaviorU_Task_Precond);
DEFINE_STAT(STAT_BehaviorU_Task_Effectors);

// ─── 数量统计
DEFINE_STAT(STAT_BehaviorU_RegisteredAgents);
DEFINE_STAT(STAT_BehaviorU_ActiveAgents);
