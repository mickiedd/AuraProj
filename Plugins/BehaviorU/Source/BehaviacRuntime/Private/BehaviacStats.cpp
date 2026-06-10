// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviacStats.h"

// ─── 子系统级
DEFINE_STAT(STAT_Behaviac_Tick);
DEFINE_STAT(STAT_Behaviac_Phase1);
DEFINE_STAT(STAT_Behaviac_Phase2);
DEFINE_STAT(STAT_Behaviac_Phase2_Wait);

// ─── Agent 级（游戏线程）
DEFINE_STAT(STAT_Behaviac_ProcessCmdQueue);
DEFINE_STAT(STAT_Behaviac_ExecuteMethod);

// ─── Agent 级（工作线程）
DEFINE_STAT(STAT_Behaviac_TickBT);

// ─── 工作线程
DEFINE_STAT(STAT_Behaviac_WorkerTickAgents);

// ─── 任务级
DEFINE_STAT(STAT_Behaviac_Task_Execute);
DEFINE_STAT(STAT_Behaviac_Task_Precond);
DEFINE_STAT(STAT_Behaviac_Task_Effectors);

// ─── 数量统计
DEFINE_STAT(STAT_Behaviac_RegisteredAgents);
DEFINE_STAT(STAT_Behaviac_ActiveAgents);
