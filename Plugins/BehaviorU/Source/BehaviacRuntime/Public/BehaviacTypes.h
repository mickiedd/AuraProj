// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviacTypes.generated.h"

// Logging category
BEHAVIACRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogBehaviac, Log, All);

/**
 * Console variable that controls verbose Behaviac debug logging.
 * Enable at runtime:  Behaviac.VerboseLogging 1
 * Or in DefaultEngine.ini:
 *   [SystemSettings]
 *   Behaviac.VerboseLogging=1
 */
BEHAVIACRUNTIME_API extern TAutoConsoleVariable<int32> CVarBehaviacVerboseLogging;

/**
 * Verbose log macro — only emits when Behaviac.VerboseLogging is non-zero.
 * Use this instead of UE_LOG(LogTemp, Warning, ...) for noisy per-frame / init spam.
 * Real errors should still use UE_LOG(LogBehaviac, Error, ...) directly.
 *
 * 日志前缀：[GT] = 游戏线程，[WT:id] = 工作线程（id 为系统线程 ID）
 * 可安全在任意线程调用（使用 GetValueOnAnyThread）。
 */
FORCEINLINE FString Behaviac_GetThreadTag()
{
	if (IsInGameThread())
	{
		return TEXT("[GT]");
	}
	return FString::Printf(TEXT("[WT:%u]"), FPlatformTLS::GetCurrentThreadId());
}

#define BEHAVIAC_VLOG(Format, ...) \
	if (CVarBehaviacVerboseLogging.GetValueOnAnyThread() != 0) \
	{ \
		UE_LOG(LogBehaviac, Log, TEXT("%s ") Format, *Behaviac_GetThreadTag(), ##__VA_ARGS__); \
	}

/** Return values of node execution and valid states for behaviors. */
UENUM(BlueprintType)
enum class EBehaviacStatus : uint8
{
	Invalid		UMETA(DisplayName = "Invalid"),
	Success		UMETA(DisplayName = "Success"),
	Failure		UMETA(DisplayName = "Failure"),
	Running		UMETA(DisplayName = "Running"),
};

/** Action result for preconditions/effectors. */
UENUM(BlueprintType)
enum class EBehaviacActionResult : uint8
{
	Success		UMETA(DisplayName = "Success"),
	Failure		UMETA(DisplayName = "Failure"),
	All			UMETA(DisplayName = "All"),
};

/** Operator types used in conditions and computations. */
UENUM(BlueprintType)
enum class EBehaviacOperatorType : uint8
{
	Invalid,
	Assign,			// =
	Add,			// +
	Subtract,		// -
	Multiply,		// *
	Divide,			// /
	Equal,			// ==
	NotEqual,		// !=
	Greater,		// >
	Less,			// <
	GreaterEqual,	// >=
	LessEqual,		// <=
};

/**
 * 并行节点策略：定义子节点完成时的行为
 * 类似于逻辑门的概念：AND（全部成功）、OR（任一成功）、XOR（竞速）
 */
UENUM(BlueprintType)
enum class EBehaviacParallelPolicy : uint8
{
	/**
	 * AND门策略：要求所有子节点都成功
	 * - 任一子节点失败 → 立即失败
	 * - 所有子节点成功 → 成功
	 * 用途：必须同时完成多个任务（如"巡逻且警戒"）
	 */
	RequireAll		UMETA(DisplayName = "Require All (AND)"),

	/**
	 * OR门策略：只需任一子节点成功
	 * - 任一子节点成功 → 立即成功
	 * - 所有子节点失败 → 失败
	 * 用途：多个备选方案，任一成功即可（如"寻找掩体或呼叫支援"）
	 */
	RequireOne		UMETA(DisplayName = "Require One (OR)"),

	/**
	 * 竞速策略：第一个完成的子节点决定结果
	 * - 任一子节点成功 → 立即成功
	 * - 任一子节点失败 → 立即失败
	 * 用途：竞争性任务，谁先完成听谁的（如"先到达目标点或先被击中"）
	 */
	FirstCompletes	UMETA(DisplayName = "First Completes (Race)"),
};

/**
 * 并行节点子节点完成策略：控制已完成的子节点是否继续执行
 *
 * ⚠️ 重要：此策略与 ParallelPolicy 配合使用，整体行为取决于两者的组合
 *
 * Loop 模式：子节点完成后下一帧重新执行（适用于持续任务如状态更新、监控）
 *   - 子节点返回 Success/Failure 后会被 Reset，下一帧重新执行
 *   - ⚠️ 注意：如果 SuccessPolicy/FailurePolicy 条件满足，整个 Parallel 会立即完成，Loop 重置不会生效
 *   - 示例：Loop + RequireAll → 两个任务都持续执行，直到外部中断
 *
 * Once 模式：子节点完成后缓存结果，不再执行（适用于一次性任务如初始化、竞速）
 *   - 子节点返回 Success/Failure 后缓存状态，后续帧直接使用缓存结果
 *   - 示例：Once + RequireOne → 任一成功则整个 Parallel 成功
 *
 * 典型场景：
 * 1. Loop + RequireAll：持续监控任务（如"巡逻且警戒"）
 * 2. Once + RequireOne：备选方案（如"寻找掩体或呼叫支援"）
 * 3. Once + FirstCompletes：竞速任务（如"先到达目标点或先被击中"）
 */
UENUM(BlueprintType)
enum class EBehaviacChildFinishPolicy : uint8
{
	/** 循环执行：子节点完成后下一帧重新执行（适用于持续任务）。⚠️ 注意：如果 SuccessPolicy/FailurePolicy 条件满足，整个 Parallel 会立即完成，Loop 不会生效 */
	Loop		UMETA(DisplayName = "Loop (循环执行)"),

	/** 单次执行：子节点完成后缓存结果不再执行（适用于一次性任务） */
	Once		UMETA(DisplayName = "Once (单次执行)"),
};

/** Precondition phase. */
UENUM(BlueprintType)
enum class EBehaviacPreconditionPhase : uint8
{
	Enter,
	Update,
	Both,
};

/** Effector phase. */
UENUM(BlueprintType)
enum class EBehaviacEffectorPhase : uint8
{
	Success,
	Failure,
	Both,
};

/** File format for behavior tree data. */
UENUM(BlueprintType)
enum class EBehaviacFileFormat : uint8
{
	XML,
	BSON,
};

/** Invalid node ID constant. */
#define BEHAVIAC_INVALID_NODE_ID (-2)

// ===================================================================
// FVector 黑板辅助工具
// ===================================================================

/**
 * 判断字符串是否表示 FVector（包含 X= Y= Z= 三个分量）。
 * 与 UE 的 FVector::ToString() 序列化格式保持一致：
 *   "X=1.000000 Y=2.000000 Z=3.000000"
 */
FORCEINLINE bool BehaviacIsVectorString(const FString& Str)
{
	return Str.Contains(TEXT("X=")) && Str.Contains(TEXT("Y=")) && Str.Contains(TEXT("Z="));
}

/**
 * 将 FVector 序列化为黑板字符串。
 * 使用 UE 标准格式："X=1.000000 Y=2.000000 Z=3.000000"
 */
FORCEINLINE FString BehaviacVectorToString(const FVector& V)
{
	return V.ToString();
}

/**
 * 从黑板字符串解析 FVector。
 * 解析失败时返回 FVector::ZeroVector。
 */
FORCEINLINE FVector BehaviacStringToVector(const FString& Str)
{
	FVector Result = FVector::ZeroVector;
	Result.InitFromString(Str);
	return Result;
}

/** Property container used during node loading. */
USTRUCT(BlueprintType)
struct BEHAVIACRUNTIME_API FBehaviacProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac")
	FString Value;

	FBehaviacProperty() {}
	FBehaviacProperty(const FString& InName, const FString& InValue)
		: Name(InName), Value(InValue)
	{}
};
