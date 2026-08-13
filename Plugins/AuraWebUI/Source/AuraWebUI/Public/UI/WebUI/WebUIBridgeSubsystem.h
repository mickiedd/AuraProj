// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WebUIBridgeSubsystem.generated.h"

class FWebUIWebSocketServer;
class FJsonObject;
class FJsonValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWebUICommandSignature, const FString&, Command, const FString&, PayloadJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebUIConnectionSignature, bool, bConnected);

/**
 * World-scoped bridge between the in-game Web UI and Unreal gameplay code.
 *
 * The bridge listens only on 127.0.0.1.  Browser messages are JSON objects of
 * the form {"type":"command","command":"...","payload":{...}} and are
 * surfaced to Blueprint/native listeners without giving the page arbitrary
 * UObject access.
 */
UCLASS()
class AURAWEBUI_API UWebUIBridgeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 DefaultPort = 18765;
	~UWebUIBridgeSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	bool StartServer(int32 Port = 18765);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void StopServer();

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	bool IsServerRunning() const;

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	int32 GetServerPort() const;

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	int32 GetConnectedClientCount() const;

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	FString GetWebSocketUrl() const;

	/** Broadcast a JSON event to every connected page. PayloadJson must be a JSON value. */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	bool SendEvent(const FString& EventName, const FString& PayloadJson = TEXT("{}"));

	/** Broadcast an already serialized JSON object. */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	bool SendRawJson(const FString& MessageJson);

	UPROPERTY(BlueprintAssignable, Category = "Web UI|Bridge")
	FWebUICommandSignature OnCommand;

	UPROPERTY(BlueprintAssignable, Category = "Web UI|Bridge")
	FWebUIConnectionSignature OnConnectionChanged;

private:
	FWebUIWebSocketServer* Server = nullptr;

	void HandleMessage(const FString& Message);
	void HandleConnectionChanged(bool bConnected);
	static bool ParseJsonObject(const FString& Message, TSharedPtr<FJsonObject>& OutObject);
	static FString SerializeJsonValue(const TSharedPtr<FJsonValue>& Value);
	static FString MakeServerStateJson(const UWebUIBridgeSubsystem& Bridge);
};
