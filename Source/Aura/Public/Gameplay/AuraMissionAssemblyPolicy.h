// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"
enum class EAuraAssemblyStream:uint8{Assembly,Encounter,Offer,Visual};
struct FAuraAssemblyMatrixRow{FString Key;bool bMutator=false;};
class AURA_API FAuraAssemblyRandomStreams final
{public:explicit FAuraAssemblyRandomStreams(uint32 Seed=1):BaseSeed(Seed){} uint32 Next(EAuraAssemblyStream Stream);int32 Counter(EAuraAssemblyStream Stream)const;private:uint32 BaseSeed;int32 Counters[4]={0,0,0,0};};
/** Inert Day 55 deterministic assembly/consent policy. */
class AURA_API FAuraMissionAssemblyPolicy final
{public:bool Initialize(bool Authority,int32 Participants,uint32 Seed,FString ContentHash,FString&Error);bool Confirm(bool Authority,FName Member,bool Accepted,FString&Error);bool CanLaunch()const;static bool Compatible(FName Template,FName Layout,FName Mutator,bool EscortBlocked,bool RequiredInteractionMissing,bool AnchorsUndersized);static bool MutatorWithinCaps(FName Mutator,int32 Existing,int32 Added,int32 Cap,double CueLead,bool AuraViable,bool BungeeViable);static bool ValidateMatrix(const TArray<FAuraAssemblyMatrixRow>&Rows,FString&Error);FAuraAssemblyRandomStreams& Streams(){return Random;}private:bool bAuthority=false;int32 ParticipantCount=0;TMap<FName,bool>Confirmations;FAuraAssemblyRandomStreams Random;};
