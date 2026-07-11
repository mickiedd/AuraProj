# BehaviorU UE5 Plugin

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

1. Copy the `BehaviorUPlugin` folder into your UE5 project's `Plugins/` directory
2. Regenerate project files
3. Enable the plugin in Edit > Plugins > AI > BehaviorU

## Quick Start (C++)

```cpp
// In your Actor's header
UPROPERTY(VisibleAnywhere)
UBehaviorUAgentComponent* BehaviorUAgent;

// In constructor
BehaviorUAgent = CreateDefaultSubobject<UBehaviorUAgentComponent>(TEXT("BehaviorUAgent"));

// In BeginPlay
BehaviorUAgent->SetIntProperty("health", 100);
BehaviorUAgent->SetFloatProperty("speed", 5.0f);

BehaviorUAgent->RegisterMethodHandler("SayHello", []() {
    UE_LOG(LogTemp, Log, TEXT("Hello from BehaviorU!"));
    return EBehaviorUStatus::Success;
});

// Load behavior tree asset
BehaviorUAgent->LoadBehaviorTree(MyBehaviorTreeAsset);
```

## Quick Start (Blueprint)

1. Add a **BehaviorU Agent** component to your Actor
2. Set the **Default Behavior Tree** property to your BT asset
3. Use **Set Property Value** / **Get Property Value** for the blackboard
4. Implement **On Execute Method** to handle action calls
5. The tree ticks automatically each frame (disable with `bAutoTick = false`)

## Importing Existing behavioru Trees

The plugin includes an XML import factory. In the Content Browser:
1. Right-click > Import
2. Select `.xml` behavior tree files exported from the behavioru designer
3. The importer creates `UBehaviorUBehaviorTree` assets automatically

## Architecture

| Original (C++ standalone) | UE5 Plugin |
|---------------------------|------------|
| `behavioru::Agent` | `UBehaviorUAgentComponent` (UActorComponent) |
| `behavioru::BehaviorTree` | `UBehaviorUBehaviorTree` (UDataAsset) |
| `behavioru::BehaviorNode` | `UBehaviorUBehaviorNode` (UObject) |
| `behavioru::BehaviorTask` | `UBehaviorUBehaviorTask` (UObject) |
| `behavioru::Workspace` | Integrated into Agent + subsystem |
| `std::vector` / `std::map` | `TArray` / `TMap` |
| `std::string` | `FString` |
| Custom memory allocator | `FMemory` (UE5 default) |
| Custom mutex/threading | `FCriticalSection` / `FScopeLock` |
| Custom file I/O | `FFileHelper` / `IPlatformFile` |
| Custom RTTI | UE5 `UObject` reflection |

## Module Structure

- **BehaviorURuntime**: Core runtime module (behavior trees, FSM, HTN, agent component)
- **BehaviorUEditor**: Editor module (asset factories, XML import)

## License

BSD 3-Clause License.
