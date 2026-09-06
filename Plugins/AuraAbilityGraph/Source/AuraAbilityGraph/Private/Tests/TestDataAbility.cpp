// Copyright Druid Mechanics

#include "Tests/TestDataAbility.h"
#include "AbilityDefinition.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Nodes/AbilityActionTask.h"
#include "Nodes/Composites/SequenceNode.h"

void UTestDataAbility::InitTestOwner(
    AActor* Owner,
    int32 AbilityLevel,
    FGameplayTag ReplicatedAbilityTag,
    UAuraAbilityDefinition* SourceDefinition)
{
    if (!Owner)
    {
        return;
    }
    // Set fields directly rather than InitFromActor (which asserts on a null ASC).
    ActorInfo.OwnerActor = Owner;
    ActorInfo.AvatarActor = Owner;

    if (AbilityLevel > 1)
    {
        TestAbilitySystemComponent = NewObject<UAbilitySystemComponent>(Owner);
        if (TestAbilitySystemComponent)
        {
            TestAbilitySystemComponent->RegisterComponent();
            TestAbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);
            FGameplayAbilitySpec Spec(GetClass(), AbilityLevel);
            if (SourceDefinition)
            {
                Spec.SourceObject = SourceDefinition;
            }
            if (ReplicatedAbilityTag.IsValid())
            {
                Spec.GetDynamicSpecSourceTags().AddTag(ReplicatedAbilityTag);
            }
            const FGameplayAbilitySpecHandle Handle = TestAbilitySystemComponent->GiveAbility(Spec);
            ActorInfo.AbilitySystemComponent = TestAbilitySystemComponent;
            SetCurrentActorInfo(Handle, &ActorInfo);
            return;
        }
    }

    // With no ASC/spec, GetAbilityLevel() returns 1 and the spawn nodes still
    // have the owner/avatar fields they dereference.
    SetCurrentActorInfo(FGameplayAbilitySpecHandle{}, &ActorInfo);
}

void UTestDataAbility::ActivateForTest()
{
    if (TestAbilitySystemComponent && CurrentSpecHandle.IsValid())
    {
        if (FGameplayAbilitySpec* Spec = TestAbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
        {
            // Direct invocation bypasses GAS's normal activation bookkeeping. Mark
            // the fixture active so AbilityTask delegate/EndAbility behavior matches
            // a real activation while the test exercises UAuraDataAbility itself.
            Spec->ActiveCount = 1;
        }
    }
    bIsActive = true;
    ActivateAbility(CurrentSpecHandle, &ActorInfo, CurrentActivationInfo, nullptr);
}

bool UTestDataAbility::ActivateGraphFromChildForTest(UAuraAbilityDefinition* Definition, int32 ChildIndex)
{
    if (!Definition || !Definition->RootNode || !TestAbilitySystemComponent || !CurrentSpecHandle.IsValid()
        || ChildIndex < 0 || ChildIndex >= Definition->RootNode->Children.Num())
    {
        return false;
    }

    if (FGameplayAbilitySpec* Spec = TestAbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
    {
        Spec->ActiveCount = 1;
    }
    bIsActive = true;

    PersistentCtx = FAuraAbilityExecutionContext();
    PersistentCtx.ASC = TestAbilitySystemComponent;
    PersistentCtx.AvatarActor = ActorInfo.AvatarActor.Get();
    PersistentCtx.SpecHandle = CurrentSpecHandle;
    PersistentCtx.Definition = Definition;
    PersistentCtx.NodeDef = Definition->RootNode;

    RootTask = NewObject<UAuraSequenceTask>(this);
    if (!RootTask)
    {
        return false;
    }
    RootTask->Init(const_cast<UAuraAbilityActionNode*>(Definition->RootNode.Get()), this);
    RootTask->ActiveChildIndex = ChildIndex;
    bGraphActive = true;

    const EAuraAbilityActionStatus Status = RootTask->Execute(PersistentCtx);
    if (Status != EAuraAbilityActionStatus::Running)
    {
        EndAbility(CurrentSpecHandle, &ActorInfo, CurrentActivationInfo, true, Status == EAuraAbilityActionStatus::Failure);
        return false;
    }
    return true;
}

void UTestDataAbility::EndForTest()
{
    if (bGraphActive || IsValid(RootTask))
    {
        EndAbility(CurrentSpecHandle, &ActorInfo, CurrentActivationInfo, true, true);
    }

    if (TestAbilitySystemComponent && CurrentSpecHandle.IsValid())
    {
        if (FGameplayAbilitySpec* Spec = TestAbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
        {
            Spec->ActiveCount = 0;
        }
    }
}

bool UTestDataAbility::IsGraphRunningForTest() const
{
    return bGraphActive && IsValid(RootTask);
}

bool UTestDataAbility::HasPendingTargetDataForTest() const
{
    return PendingTargetDataTask.IsValid();
}

bool UTestDataAbility::HasReplicatedTagOnlySpecForTest() const
{
    if (!TestAbilitySystemComponent || !CurrentSpecHandle.IsValid())
    {
        return false;
    }

    const FGameplayAbilitySpec* Spec = TestAbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
    return Spec && !Spec->SourceObject.IsValid() && Spec->GetDynamicSpecSourceTags().Num() > 0;
}

bool UTestDataAbility::SendGameplayEventForTest(const FGameplayTag& EventTag)
{
    if (!TestAbilitySystemComponent || !EventTag.IsValid())
    {
        return false;
    }

    FGameplayEventData EventData;
    TestAbilitySystemComponent->HandleGameplayEvent(EventTag, &EventData);
    return true;
}
