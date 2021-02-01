#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFrameWork/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/NetDriver.h"
#include "Math/Rotator.h"
#include "GameFramework/PlayerState.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "Net/UnrealNetwork.h"
#include "../FGRocket.h"
#include "../FGPickup.h"

const static float MaxMoveDeltaTime = 0.125f;

AFGPlayer::AFGPlayer() 
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComponent;

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(CollisionComponent);
	StaticMeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));

	SetReplicateMovement(false);
}

void AFGPlayer::BeginPlay() 
{
	Super::BeginPlay();

	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	SpawnRockets();

	ServerPlayerHealth = PlayerSettings->StartingHealth;

	BP_OnNumRocketsChanged(NumRockets);
	BP_OnPlayerHealthChanged(ServerPlayerHealth);

	OriginalMeshOffset = StaticMeshComponent->GetRelativeLocation();

}

void AFGPlayer::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);

	FireCooldownElapsed -= DeltaTime;

	if (!ensure(PlayerSettings != nullptr))
		return;

	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

	if (IsLocallyControlled()) {
		ClientTimeStamp += DeltaTime;

		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float TurnSpeedDefault = PlayerSettings->TurnSpeedDefault;
		const float BrakingFriction = PlayerSettings->BrakingFriction;
		const float DefaultFriction = PlayerSettings->DefaultFriction;

		const float Friction = IsBraking() ? BrakingFriction : DefaultFriction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (MaxVelocity * 0.75f)), 0.0f, 1.0f);

		const float TurnSpeed = FMath::InterpEaseOut(0.0f, TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;

		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection);

		AddMovementVelocity(DeltaTime);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		Server_SendMovement(GetActorLocation(), ClientTimeStamp, Forward, GetActorRotation().Yaw);
	}
	else {
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->DefaultFriction;
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		if (bPerformNetworkSmoothing)
		{
			const FVector NewRelativeLocation = FMath::VInterpTo(StaticMeshComponent->GetRelativeLocation(), OriginalMeshOffset, LastCorrectionDelta, 1.75f);
			StaticMeshComponent->SetRelativeLocation(NewRelativeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);
}

int32 AFGPlayer::GetPing() const {
	if (GetPlayerState()) {
		return static_cast<int32>(GetPlayerState()->GetPing());
	}

	return 0;
}

void AFGPlayer::SpawnRockets()
{
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;

		for (int32 Index = 0; Index < RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			RocketInstances.Add(NewRocketInstance);
		}
	}
}

void AFGPlayer::AddMovementVelocity(float DeltaTime)
{
	if (!ensure(PlayerSettings != nullptr))
		return;

	const float MaxVelocity = PlayerSettings->MaxVelocity;
	const float Acceleration = PlayerSettings->Acceleration;
	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (IsLocallyControlled())
		Server_OnPickup(Pickup);
}

void AFGPlayer::Client_OnPickupRockets_Implementation(int32 PickedUpRockets)
{
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	ServerNumRockets += Pickup->NumRockets;
	Multicast_OnPickupRockets(Pickup->NumRockets);
}

void AFGPlayer::Multicast_OnPickupRockets_Implementation(int32 PickedUpRockets)
{
	NumRockets += PickedUpRockets;
	BP_OnNumRocketsChanged(NumRockets);
}

void AFGPlayer::OnDamageTaken(int32 DamageToTake)
{
	if (IsLocallyControlled())
		Server_OnDamageTaken(DamageToTake);
}

void AFGPlayer::Client_OnDamageTaken_Implementation(int32 CorrectedPlayerHealth)
{
}

void AFGPlayer::Server_OnDamageTaken_Implementation(int32 DamageToTake)
{
	ServerPlayerHealth -= DamageToTake;
	Multicast_OnDamageTaken(ServerPlayerHealth);
}

void AFGPlayer::Multicast_OnDamageTaken_Implementation(int32 ServersPlayerHealth)
{
	PlayerHealth = ServersPlayerHealth;
	BP_OnPlayerHealthChanged(PlayerHealth);
}

void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();

	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWidget();
}

void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWidget();
}

void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	ReplicatedLocation = LocationToSend;
}

void AFGPlayer::Multicast_SendLocation_Implementation(const FVector& LocationToSend)
{
	if (!IsLocallyControlled()) {
		SetActorLocation(LocationToSend);
	}
}

int32 AFGPlayer::GetNumActiveRockets() const
{
	int32 NumActive = 0;
	for(AFGRocket* Rocket : RocketInstances)
	{
		if(!Rocket->IsFree())
			NumActive++;
	}

	return NumActive;
}

void AFGPlayer::Handle_Accelerate(float Value) {
	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value) {
	Turn = Value;
}

void AFGPlayer::Handle_BrakePressed() {
	bBrake = true;
}

void AFGPlayer::Handle_BrakeReleased() {
	bBrake = false;
}

void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;

	if (bShowDebugMenu)
		ShowDebugMenu();
	else
		HideDebugMenu();
}

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}

void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
		return;

	if (NumRockets <= 0 && !bUnlimitedRockets)
		return;

	if (GetNumActiveRockets() >= MaxActiveRockets)
		return;

	AFGRocket* NewRocket = GetFreeRocket();

	NewRocket->RocketOwner = this;

	if (!ensure(NewRocket != nullptr))
		return;

	FireCooldownElapsed = PlayerSettings->FireCooldown;

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
		else
		{
			NumRockets--;
			BP_OnNumRocketsChanged(NumRockets);
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
	}
}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& FacingRotation)
{
	if ((ServerNumRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(FacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw) * 0.5f;
		const FRotator NewFacingRotation = FacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumRockets--;
		BP_OnNumRocketsChanged(ServerNumRockets);
		Multicast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}

void AFGPlayer::Multicast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& FacingRotation)
{
	if (NumRockets <= 0)
		return;

	if (!ensure(NewRocket != nullptr))
		return;

	if (IsClient())
	{
		NewRocket->ApplyCorrection(FacingRotation.Vector());
		BP_OnNumRocketsChanged(NumRockets);
	}
	else
	{
		NumRockets--;
		BP_OnNumRocketsChanged(NumRockets);
		NewRocket->StartMoving(FacingRotation.Vector(), RocketStartLocation);
	}
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}

void AFGPlayer::Cheat_IncreaseRockets(int32 InNumRockets)
{
	if (IsLocallyControlled())
		NumRockets += InNumRockets;
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
		return;

	if (!IsLocallyControlled())
		return;

	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
			continue;

		if (Rocket->IsFree())
			return Rocket;
	}

	return nullptr;
}

void AFGPlayer::Server_SendMovement_Implementation(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	Multicast_SendMovement(ClientLocation, TimeStamp, ClientForward, ClientYaw);
}

void AFGPlayer::Multicast_SendMovement_Implementation(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	if (!IsLocallyControlled())
	{
		Forward = ClientForward;
		const float DeltaTime = FMath::Min(TimeStamp - ClientTimeStamp, MaxMoveDeltaTime);
		ClientTimeStamp = TimeStamp;
		AddMovementVelocity(DeltaTime);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ClientYaw, 0.0f));

		const FVector DeltaDiff = InClientLocation - GetActorLocation();

		if (DeltaDiff.SizeSquared() > FMath::Square(40.0f))
		{
			if (bPerformNetworkSmoothing)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(StaticMeshComponent);
				MovementComponent->UpdatedComponent->SetWorldLocation(InClientLocation, false, nullptr, ETeleportType::TeleportPhysics);
				LastCorrectionDelta = DeltaTime;

			}
			else
			{

			}
		}
	}
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLoc;
}

void AFGPlayer::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
	DOREPLIFETIME(AFGPlayer, PlayerHealth);
	//DOREPLIFETIME(AFGPlayer, NumRockets);
}
