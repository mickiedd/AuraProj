// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"

enum class EAuraBossFinalePhase:uint8 { Phase1,Phase2,Dead,Extraction,Settled,Failed };
enum class EAuraBossFinaleResult:uint8 { Rejected,NoChange,PhaseChanged,ActionStarted,AddAdmitted,BossDied,ExtractionStarted,Settled,Failed };

struct FAuraBossPatternContract { FName Id=NAME_None; double Windup=0; double CueLead=0; double Recovery=0; bool bAuraCounter=false; bool bBungeeCounter=false; bool bRoomWide=false; };

/** Inert Day 54 captain phase/add/extraction authority reducer. */
class AURA_API FAuraBossFinaleState final
{
public:
	bool Initialize(bool bAuthority,const FGuid& RunId,double Start,double RunDeadline,FString& Error);
	EAuraBossFinaleResult ObserveHealth(bool bAuthority,int32 DamageSequence,float HealthNormalized,FString& Error);
	EAuraBossFinaleResult StartAction(bool bAuthority,FName ActionId,double Now,double Deadline,FString& Error);
	EAuraBossFinaleResult AdmitReinforcement(bool bAuthority,bool bPlacementSucceeded,int32 ExistingLive,int32 LiveCap,FString& Error);
	EAuraBossFinaleResult AcceptBossDeath(bool bAuthority,int32 DeathSequence,double Now,FString& Error);
	EAuraBossFinaleResult ResolveExtractionFrame(bool bAuthority,double Now,int32 AcceptedPlayerDeaths,bool bAnyAliveConnected,bool bChannelCompleted,FString& Error);
	static bool ValidatePattern(const FAuraBossPatternContract& Pattern);
	static bool ValidateEmergencyAccess(bool bAuraReachable,bool bBungeeReachable,bool bFinaleGatesClosed,bool bSafeZoneImmunity,double PriorCooldown,double AfterCooldown);
	EAuraBossFinalePhase GetPhase()const{return Phase;} int32 GetPhase2Count()const{return Phase2Count;} int32 GetAdds()const{return Adds;} bool HasPendingAction()const{return !PendingAction.IsNone();} int32 GetSettlementCount()const{return SettlementCount;}
private:
	bool bAuthority=false; FGuid RunId; EAuraBossFinalePhase Phase=EAuraBossFinalePhase::Phase1; int32 LastDamageSequence=0; int32 BossDeathSequence=0; int32 Phase2Count=0; int32 Adds=0; FName PendingAction=NAME_None; double ActionDeadline=0; double RunDeadline=0; double ExtractionDeadline=0; int32 SettlementCount=0;
};
