// Copyright Druid Mechanics
#include "Gameplay/AuraMissionPresentationTypes.h"
bool FAuraMissionPresentationState::AcceptSnapshot(int32 R){if(R<=Revision)return false;Revision=R;return true;}
bool FAuraMissionPresentationState::ObserveGuidance(uint32 Bit,bool Accepted){if(!Accepted||Bit==0||(Bit&(Bit-1))!=0)return false;CurrentGuidance|=Bit;return true;}
bool FAuraMissionPresentationState::CommitGuidance(EAuraGuidanceCommitReason R){if(R==EAuraGuidanceCommitReason::ForcedCrash||R==EAuraGuidanceCommitReason::HubReset)return false;CommittedGuidance|=CurrentGuidance;return true;}
bool FAuraMissionPresentationState::ResetGuidanceAtHub(bool Hub){if(!Hub)return false;CurrentGuidance=CommittedGuidance=0;return true;}
void FAuraMissionPresentationState::SimulateForcedCrash(){CurrentGuidance=CommittedGuidance;}
bool FAuraMissionPresentationState::TryPing(int32 Id,double Now,bool Valid){PrunePings(Now);if(Id<=0||!FMath::IsFinite(Now)||!Valid||SeenPingRequests.Contains(Id))return false;if(!bHasPingTokenTime){LastPingTokenTime=Now;bHasPingTokenTime=true;}else{PingTokens=FMath::Min(3.0,PingTokens+FMath::Max(0.0,Now-LastPingTokenTime)*2.0);LastPingTokenTime=Now;}if(PingTokens<1.0||Pings.Num()>=3)return false;PingTokens-=1.0;SeenPingRequests.Add(Id);FAuraTacticalPing&P=Pings.AddDefaulted_GetRef();P.RequestId=Id;P.Created=Now;P.Expiry=Now+5;return true;}
void FAuraMissionPresentationState::PrunePings(double Now){for(int32 I=Pings.Num()-1;I>=0;--I)if(Now>=Pings[I].Expiry)Pings.RemoveAt(I);}
void FAuraMissionPresentationState::OpenModal(){bGameplayFocus=false;}void FAuraMissionPresentationState::CloseModal(){bGameplayFocus=true;}
bool FAuraMissionPresentationState::PayloadIsOwnerSafe(bool O,bool S,bool W,bool Owner){return Owner||(!O&&!S&&!W);}
bool FAuraMissionPresentationState::ValidateHudLayout(int32 W,int32 H,int32 Scale,bool Controls,bool Reticle,bool Keyboard){return (W==1280&&H==720||W==1920&&H==1080||W==2560&&H==1440)&&(Scale==100||Scale==125||Scale==150)&&Controls&&Reticle&&Keyboard;}
