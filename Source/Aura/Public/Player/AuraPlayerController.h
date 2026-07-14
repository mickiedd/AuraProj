// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "AuraPlayerController.generated.h"


class IHighlightInterface;
class UNiagaraSystem;
class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UAuraInputConfig;
class UAuraAbilitySystemComponent;
class UAuraAttributeSet;
class USplineComponent;
class AMagicCircle;
class AAuraBroomVehicle;
class UCharacterMovementComponent;
class UServerTravelComponent;
class UAuraClientDisconnectHandler;
class UAuraHeartbeatComponent;
class UAuraBuildingComponent;

enum class ETargetingStatus : uint8
{
	TargetingEnemy,
	TargetingNonEnemy,
	NotTargeting
};

/**
 * 
 */
UCLASS()
class AURA_API AAuraPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AAuraPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Exec)
	void FullAbilities();

	UFUNCTION(Exec)
	void ShowLocation();

	/**
	 * Teleport the controlled pawn to a randomly chosen other player's pawn.
	 * Runs server-side so every player is a candidate regardless of client-side
	 * net-relevancy culling; safe to call from either client or authority (it
	 * routes through a server RPC when invoked on a non-authoritative controller).
	 */
	void RequestTransferToRandomPlayer();

	// ---- UGC Building Exec Commands ----------------------------------------

	/**
	 * Enter placement mode for the mesh at the given asset path.
	 * Example: StartPlacement /Game/Cartoon_City_Free/Meshes/Roads/SM_road_001.SM_road_001
	 */
	UFUNCTION(Exec)
	void StartPlacement(const FString& MeshPath);

	/** Confirm and submit the current pending placement to the server. */
	UFUNCTION(Exec)
	void ConfirmPlacement();

	/** Cancel the current placement and destroy the preview ghost. */
	UFUNCTION(Exec)
	void CancelPlacement();

	/** Rotate the pending placement preview by 90 degrees per command invocation. */
	UFUNCTION(Exec)
	void RotatePlacement(float DeltaYaw);

	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter, bool bBlockedHit, bool bCriticalHit);

	UFUNCTION(BlueprintCallable)
	void ShowMagicCircle(UMaterialInterface* DecalMaterial = nullptr);

	UFUNCTION(BlueprintCallable)
	void HideMagicCircle();

	void RequestBroomMount(AAuraBroomVehicle* BroomToMount);

	/** Automation/test hook: fire one of the equipped ability slots (1-4 or LMB) with a
	 *  simulated press+release. Picks uniformly at random among slots that actually have
	 *  an ability equipped, so it is a no-op (and logged) when nothing is equipped or
	 *  ability input is blocked. Invoked by name (UObject reflection) by the AutoTest
	 *  stress harness so that plugin stays decoupled from Aura. */
	UFUNCTION(BlueprintCallable, Category = "AutoTest")
	void AutoTestUseRandomEquippedAbility();


protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerFullAbilities();

	UFUNCTION(Server, Reliable)
	void ServerTransferToRandomPlayer();

	UFUNCTION(Client, Reliable)
	void ClientRefreshAbilityUI();

	void ExecuteFullAbilities();

	/** Server-authoritative implementation of RequestTransferToRandomPlayer. */
	void ExecuteTransferToRandomPlayer();
private:
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> AuraContext;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ShiftAction;

	void ShiftPressed();
	void ShiftReleased();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bShouldSprint);

	UFUNCTION(Server, Reliable)
	void ServerRequestBroomDismount(AAuraBroomVehicle* BroomToDismount);

	UFUNCTION(Server, Reliable)
	void ServerRequestBroomMount(AAuraBroomVehicle* BroomToMount);

	UFUNCTION(Server, Unreliable)
	void ServerApplyBroomFlightInput(AAuraBroomVehicle* Broom, const FVector& WorldDirection, float ScaleValue);

	/**
	 * Rotates the broom's yaw on the server to match the camera forward direction.
	 * Unreliable: dropped packets are acceptable since each call carries the absolute yaw.
	 */
	UFUNCTION(Server, Unreliable)
	void ServerSetBroomYaw(AAuraBroomVehicle* Broom, float WorldYaw);

	/**
	 * Called on the owning client whenever the camera yaw changes (screen-edge or
	 * right-mouse drag). Routes the broom yaw update through the correct authority path.
	 */
	void UpdateMountedBroomYaw(float WorldYaw);

	void ApplySprintState(bool bShouldSprint);
	UCharacterMovementComponent* GetControlledCharacterMovement() const;

	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float SprintSpeedMultiplier = 1.5f;

	float CachedWalkSpeed = 0.f;
	bool bIsSprinting = false;
	bool bShiftKeyDown = false;
	float LastMoveInputLogTime = -1000.f;
	float LastMoveBlockedLogTime = -1000.f;

	UPROPERTY(EditDefaultsOnly, Category="Debug|Movement", meta = (ClampMin = "0.1"))
	float MoveInputLogInterval = 0.25f;

	void Move(const FInputActionValue& InputActionValue);
	void RightMousePressed();
	void RightMouseReleased();
	void RotateCameraFromMouseDelta();
	void RotateCameraFromScreenEdge(float DeltaTime);
	void ApplyGameAndUIInputMode();
	void JumpPressed();
	void JumpReleased();
	void CrouchPressed();
	void CrouchReleased();

	// Broom vertical flight (Q = descend, E = ascend). Held-key flags are sampled
	// each PlayerTick so thrust stays continuous while the key is held, matching how
	// the WASD Move action feeds AddFlightInput every tick while held.
	void BroomAscendPressed();
	void BroomAscendReleased();
	void BroomDescendPressed();
	void BroomDescendReleased();
	void ApplyBroomVerticalFlight();

	bool bBroomAscendHeld = false;
	bool bBroomDescendHeld = false;

	// 0..1 input magnitude fed to AddFlightInput for vertical flight (Q/E), mirroring
	// the clamped magnitude the WASD Move path produces. Lower it to soften climb/dive.
	UPROPERTY(EditDefaultsOnly, Category="Movement|Broom", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BroomVerticalFlightScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float RightMouseYawSpeed = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float RightMousePitchSpeed = 0.24f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float CameraPitchMin = -80.f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float CameraPitchMax = -15.f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	bool bEnableEdgeScreenCameraRotation = true;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float EdgeScreenBorderSize = 24.f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float EdgeScreenYawDegreesPerSecond = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float EdgeScreenPitchDegreesPerSecond = 90.f;

	bool bRightMouseDown = false;
	bool bCachedShowMouseCursor = true;

	void CursorTrace();
	TObjectPtr<AActor> LastActor;
	TObjectPtr<AActor> ThisActor;
	FHitResult CursorHit;
	static void HighlightActor(AActor* InActor);
	static void UnHighlightActor(AActor* InActor);

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UAuraInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UAuraAbilitySystemComponent> AuraAbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UAuraAttributeSet> AuraAttributeSet;

	UAuraAbilitySystemComponent* GetASC();
	const UAuraAttributeSet* GetAuraAS() const;
	bool IsAbilityInputReady() const;
	bool HasEquippedAbilityForInputTag(const FGameplayTag& InputTag);

	
	FVector CachedDestination = FVector::ZeroVector;
	float FollowTime = 0.f;
	float ShortPressThreshold = 0.5f;
	bool bAutoRunning = false;
	ETargetingStatus TargetingStatus = ETargetingStatus::NotTargeting;

	UPROPERTY(EditDefaultsOnly)
	float AutoRunAcceptanceRadius = 50.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UNiagaraSystem> ClickNiagaraSystem;

	void AutoRun();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	UPROPERTY(VisibleAnywhere, Category = "Network")
	TObjectPtr<UServerTravelComponent> ServerTravelComponent;

	/** Detects mid-game server-connection loss and routes the client back to the Login level. */
	UPROPERTY(VisibleAnywhere, Category = "Network")
	TObjectPtr<UAuraClientDisconnectHandler> ClientDisconnectHandler;

	/** Application-level heartbeat; the disconnect handler binds to its OnHeartbeatLost. */
	UPROPERTY(VisibleAnywhere, Category = "Network")
	TObjectPtr<UAuraHeartbeatComponent> HeartbeatComponent;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AMagicCircle> MagicCircleClass;

	UPROPERTY()
	TObjectPtr<AMagicCircle> MagicCircle;

	void UpdateMagicCircleLocation();
};
