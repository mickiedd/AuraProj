// Copyright Druid Mechanics
#include "Gameplay/AuraBossFinaleTypes.h"

bool FAuraBossFinaleState::Initialize(bool A,const FGuid& R,double Start,double Deadline,FString& E)
{ E.Reset(); *this=FAuraBossFinaleState(); if(!A||!R.IsValid()||!FMath::IsFinite(Start)||!FMath::IsFinite(Deadline)||Deadline<=Start){E=TEXT("BossIdentityInvalid");return false;} bAuthority=true;RunId=R;RunDeadline=Deadline;return true; }
EAuraBossFinaleResult FAuraBossFinaleState::ObserveHealth(bool A,int32 Seq,float H,FString& E)
{ E.Reset(); if(!bAuthority||!A||Phase==EAuraBossFinalePhase::Dead||Seq<=LastDamageSequence||!FMath::IsWithinInclusive(H,0.f,1.f)){E=TEXT("BossHealthRejected");return EAuraBossFinaleResult::Rejected;} LastDamageSequence=Seq;if(Phase==EAuraBossFinalePhase::Phase1&&H<=.5f){Phase=EAuraBossFinalePhase::Phase2;++Phase2Count;return EAuraBossFinaleResult::PhaseChanged;}return EAuraBossFinaleResult::NoChange; }
EAuraBossFinaleResult FAuraBossFinaleState::StartAction(bool A,FName Id,double Now,double Deadline,FString& E)
{ E.Reset();if(!bAuthority||!A||Id.IsNone()||!FMath::IsFinite(Now)||!FMath::IsFinite(Deadline)||Deadline<=Now||(Phase!=EAuraBossFinalePhase::Phase1&&Phase!=EAuraBossFinalePhase::Phase2)||!PendingAction.IsNone()){E=TEXT("BossActionRejected");return EAuraBossFinaleResult::Rejected;}PendingAction=Id;ActionDeadline=Deadline;return EAuraBossFinaleResult::ActionStarted; }
EAuraBossFinaleResult FAuraBossFinaleState::AdmitReinforcement(bool A,bool Placement,int32 Existing,int32 Cap,FString& E)
{ E.Reset();if(!bAuthority||!A||(Phase!=EAuraBossFinalePhase::Phase1&&Phase!=EAuraBossFinalePhase::Phase2)||Existing<0||Cap<=0||Adds>=4||Existing>=Cap){E=TEXT("BossAddRejected");return EAuraBossFinaleResult::Rejected;}if(!Placement)return EAuraBossFinaleResult::NoChange;++Adds;return EAuraBossFinaleResult::AddAdmitted; }
EAuraBossFinaleResult FAuraBossFinaleState::AcceptBossDeath(bool A,int32 Seq,double Now,FString& E)
{ E.Reset();if(!bAuthority||!A||Seq<=0||BossDeathSequence>0||!FMath::IsFinite(Now)||Now>=RunDeadline){E=TEXT("BossDeathRejected");return EAuraBossFinaleResult::Rejected;}BossDeathSequence=Seq;PendingAction=NAME_None;ActionDeadline=0;Phase=EAuraBossFinalePhase::Extraction;ExtractionDeadline=FMath::Min(RunDeadline,Now+30.0);return EAuraBossFinaleResult::ExtractionStarted; }
EAuraBossFinaleResult FAuraBossFinaleState::ResolveExtractionFrame(bool A,double Now,int32 Deaths,bool Alive,bool Complete,FString& E)
{ E.Reset();if(!bAuthority||!A||Phase!=EAuraBossFinalePhase::Extraction||!FMath::IsFinite(Now)||Deaths<0){E=TEXT("ExtractionRejected");return EAuraBossFinaleResult::Rejected;}if(Deaths>0&&!Alive){Phase=EAuraBossFinalePhase::Failed;return EAuraBossFinaleResult::Failed;}if(Now>=RunDeadline||Now>=ExtractionDeadline){Phase=EAuraBossFinalePhase::Failed;return EAuraBossFinaleResult::Failed;}if(Complete&&Alive){Phase=EAuraBossFinalePhase::Settled;++SettlementCount;return EAuraBossFinaleResult::Settled;}return EAuraBossFinaleResult::NoChange; }
bool FAuraBossFinaleState::ValidatePattern(const FAuraBossPatternContract& P)
{ return !P.Id.IsNone()&&P.Windup>=.85&&P.CueLead>=.65&&P.Recovery>=1.2&&P.bAuraCounter&&P.bBungeeCounter&&!P.bRoomWide; }
bool FAuraBossFinaleState::ValidateEmergencyAccess(bool A,bool B,bool Gates,bool Immunity,double Before,double After)
{ return A&&B&&Gates&&!Immunity&&FMath::IsNearlyEqual(Before,After); }
