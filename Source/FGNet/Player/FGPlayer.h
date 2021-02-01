#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;
class AFGRocket;
class AFGPickup;

UCLASS()
class FGNET_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:
	AFGPlayer();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UPROPERTY(EditAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;

	//Rockets
	void OnPickup(AFGPickup* Pickup);

	UFUNCTION(Server, Reliable)
	void Server_OnPickup(AFGPickup* Pickup);

	UFUNCTION(Client, Reliable)
	void Client_OnPickupRockets(int32 PickedUpRockets);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnPickupRockets(int32 PickedUpRockets);

	//Transform
	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendLocation(const FVector& LocationToSend);

	//Damage
	void OnDamageTaken(int32 DamageToTake);

	UFUNCTION(Client, Reliable)
	void Client_OnDamageTaken(int32 CorrectedPlayerHealth);

	UFUNCTION(Server, Reliable)
	void Server_OnDamageTaken(int32 DamageToTake);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDamageTaken(int32 DamageToTake);

	void ShowDebugMenu();
	void HideDebugMenu();

	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumRockets; }

	UFUNCTION(BlueprintPure)
	int32 GetPlayerHealth() const { return PlayerHealth; }

	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Num Rockets Changed"))
    void BP_OnNumRocketsChanged(int32 NewNumRockets);

	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Player Health Changed"))
	void BP_OnPlayerHealthChanged(int32 NewPlayerHealth);

	int32 GetNumActiveRockets() const;

	void FireRocket();

	void SpawnRockets();

private:
	void AddMovementVelocity(float DeltaTime);

	//Health
	int32 ServerPlayerHealth = 0;

	UPROPERTY(Replicated)
	int32 PlayerHealth = 0;

	//Rockets
	int32 ServerNumRockets = 0;

	UPROPERTY(Replicated)
	int32 NumRockets = 0;

	FVector GetRocketStartLocation() const;

	AFGRocket* GetFreeRocket() const;

	bool IsClient() { return GetLocalRole() == ROLE_AutonomousProxy; }

	UFUNCTION(Server, Unreliable)
	void Server_SendMovement(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendMovement(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw);

	UFUNCTION(Server, Reliable)
	void Server_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(Client, Reliable)
	void Client_RemoveRocket(AFGRocket* RocketToRemove);

	UFUNCTION(BlueprintCallable)
	void Cheat_IncreaseRockets(int32 InNumRockets);

	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;

	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;

	int32 MaxActiveRockets = 3;

	float FireCooldownElapsed = 0.0f;

	UPROPERTY(EditAnywhere, Category = Weapon)
	bool bUnlimitedRockets = false;

	//Controls
	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();
	void Handle_DebugMenuPressed();
	void Handle_FirePressed();

	//Movement
	UPROPERTY(Replicated)
	float ReplicatedYaw = 0.0f;

	UPROPERTY(Replicated)
	FVector ReplicatedLocation;

	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	bool bBrake = false;

	float ClientTimeStamp = 0.0f;
	float ServerTimeStamp = 0.0f;
	float LastCorrectionDelta = 0.0f;

	UPROPERTY(EditAnywhere, Category = Network)
	bool bPerformNetworkSmoothing = true;

	FVector OriginalMeshOffset = FVector::ZeroVector;

	FVector InterpLocation;
	FVector TargetLocation;
	UPROPERTY(EditAnywhere, category = "Network")
	float InterpSpeed = 1.0f;

	//Debugmenu
	void CreateDebugWidget();
	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;

	bool bShowDebugMenu = false;

	//Components
	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	USpringArmComponent* SpringArmComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Movement)
	UFGMovementComponent* MovementComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* StaticMeshComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* CollisionComponent;
};
