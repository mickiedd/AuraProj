// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"
enum class EAuraGuidanceCommitReason:uint8{NormalTerminal,MemberForfeit,LeaseExpiry,GracefulShutdown,ForcedCrash,HubReset};
struct FAuraTacticalPing{int32 RequestId=0;double Created=0;double Expiry=0;};
/** Inert Day 56 revision, guidance, ping and focus contracts. */
class AURA_API FAuraMissionPresentationState final
{public:bool AcceptSnapshot(int32 Revision);bool ObserveGuidance(uint32 Bit,bool AcceptedAction);bool CommitGuidance(EAuraGuidanceCommitReason Reason);bool ResetGuidanceAtHub(bool bAtHub);void SimulateForcedCrash();bool TryPing(int32 RequestId,double Now,bool RangeAndLosValid);void PrunePings(double Now);void OpenModal();void CloseModal();static bool PayloadIsOwnerSafe(bool ContainsOffers,bool ContainsSupplyLedger,bool ContainsWallet,bool IsOwnerPayload);static bool ValidateHudLayout(int32 Width,int32 Height,int32 TextScale,bool RequiredControlsVisible,bool ReticleClear,bool KeyboardReachable);int32 GetRevision()const{return Revision;}uint32 GetCurrentGuidance()const{return CurrentGuidance;}uint32 GetCommittedGuidance()const{return CommittedGuidance;}uint32 GetLegacyTutorial()const{return LegacyTutorial;}int32 GetActivePings()const{return Pings.Num();}bool HasGameplayFocus()const{return bGameplayFocus;}private:int32 Revision=0;uint32 CurrentGuidance=0;uint32 CommittedGuidance=0;uint32 LegacyTutorial=0x15;TArray<FAuraTacticalPing>Pings;TSet<int32>SeenPingRequests;double PingTokens=3.0;double LastPingTokenTime=0.0;bool bHasPingTokenTime=false;bool bGameplayFocus=true;};
