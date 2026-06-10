# Behaviac UE5 Plugin

A comprehensive Unreal Engine 5 plugin for game AI, supporting **Behavior Trees**, **Finite State Machines (FSM)**, and **Hierarchical Task Networks (HTN)**.

## Features

### Behavior Tree (41 node types)
- **Composites (11)**: Selector, Sequence, Parallel, IfElse, SelectorLoop, SelectorProbability, SelectorStochastic, SequenceStochastic, ReferenceBehavior (sub-trees), WithPrecondition
- **Actions (8)**: Action, Assignment, Compute, Noop, End, Wait, WaitFrames, WaitForSignal
- **Conditions (6)**: Condition, And, Or, True, False, ConditionBase
- **Decorators (16)**: AlwaysFailure, AlwaysRunning, AlwaysSuccess, Not, Loop, LoopUntil, Repeat, Count, CountLimit, Time, Frames, FailureUntil, SuccessUntil, Iterator, Log, Weight
- **Attachments (3)**: Precondition, Effector, Event

### Finite State Machine
- States with enter/exit actions
- Condition-based transitions
- Wait states (time and frame-based)
- Always transitions (unconditional)
- Final states

### Hierarchical Task Network
- Task decomposition planner
- Compound tasks with multiple methods
- Automatic replanning on failure
- Preconditions for method selection

## Installation

1. Copy the `BehaviacPlugin` folder into your UE5 project's `Plugins/` directory
2. Regenerate project files
3. Enable the plugin in Edit > Plugins > AI > Behaviac

## Quick Start (C++)

```cpp
// In your Actor's header
UPROPERTY(VisibleAnywhere)
UBehaviacAgentComponent* BehaviacAgent;

// In constructor
BehaviacAgent = CreateDefaultSubobject<UBehaviacAgentComponent>(TEXT("BehaviacAgent"));

// In BeginPlay
BehaviacAgent->SetIntProperty("health", 100);
BehaviacAgent->SetFloatProperty("speed", 5.0f);

BehaviacAgent->RegisterMethodHandler("SayHello", []() {
    UE_LOG(LogTemp, Log, TEXT("Hello from Behaviac!"));
    return EBehaviacStatus::Success;
});

// Load behavior tree asset
BehaviacAgent->LoadBehaviorTree(MyBehaviorTreeAsset);
```

## Quick Start (Blueprint)

1. Add a **Behaviac Agent** component to your Actor
2. Set the **Default Behavior Tree** property to your BT asset
3. Use **Set Property Value** / **Get Property Value** for the blackboard
4. Implement **On Execute Method** to handle action calls
5. The tree ticks automatically each frame (disable with `bAutoTick = false`)

## Importing Existing behaviac Trees

The plugin includes an XML import factory. In the Content Browser:
1. Right-click > Import
2. Select `.xml` behavior tree files exported from the behaviac designer
3. The importer creates `UBehaviacBehaviorTree` assets automatically

## Architecture

| Original (C++ standalone) | UE5 Plugin |
|---------------------------|------------|
| `behaviac::Agent` | `UBehaviacAgentComponent` (UActorComponent) |
| `behaviac::BehaviorTree` | `UBehaviacBehaviorTree` (UDataAsset) |
| `behaviac::BehaviorNode` | `UBehaviacBehaviorNode` (UObject) |
| `behaviac::BehaviorTask` | `UBehaviacBehaviorTask` (UObject) |
| `behaviac::Workspace` | Integrated into Agent + subsystem |
| `std::vector` / `std::map` | `TArray` / `TMap` |
| `std::string` | `FString` |
| Custom memory allocator | `FMemory` (UE5 default) |
| Custom mutex/threading | `FCriticalSection` / `FScopeLock` |
| Custom file I/O | `FFileHelper` / `IPlatformFile` |
| Custom RTTI | UE5 `UObject` reflection |

## Module Structure

- **BehaviacRuntime**: Core runtime module (behavior trees, FSM, HTN, agent component)
- **BehaviacEditor**: Editor module (asset factories, XML import)

## License

BSD 3-Clause License.
