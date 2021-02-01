#pragma once

#include "GameFramework/Actor.h"
#include "Player/FGPlayer.h"
#include "FGRocket.generated.h"

class UStaticMeshComponent;

UCLASS()
class FGNET_API AFGRocket : public AActor
{
	GENERATED_BODY()
public:
	AFGRocket();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void StartMoving(const FVector& Forward, const FVector& InStartLocation);
	void ApplyCorrection(const FVector& Forward);

	bool IsFree() const { return bIsFree; }

	void Explode();
	void MakeFree();

	UPROPERTY(EditAnywhere, Category = Damage)
	int32 DamageValue = 5;

	AFGPlayer* RocketOwner;

	void Collide(FHitResult other);
private:
	void SetRocketVisibility(bool bVisible);

	FCollisionQueryParams CachedCollisionQueryParams;

	UPROPERTY(EditAnywhere, Category = VFX)
		UParticleSystem* Explosion = nullptr;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		UStaticMeshComponent* MeshComponent = nullptr;

	UPROPERTY(EditAnywhere, Category = Debug)
		bool bDebugDrawCorrection = true;

	FVector OriginalFacingDirection = FVector::ZeroVector;

	FVector FacingRotationStart = FVector::ZeroVector;
	FQuat FacingRotationCorrection = FQuat::Identity;

	FVector RocketStartLocation = FVector::ZeroVector;

	float LifeTime = 2.0f;
	float LifeTimeElapsed = 0.0f;

	float DistanceMoved = 0.0f;

	UPROPERTY(EditAnywhere)
		float MovementVelocity = 1300.0f;

	bool bIsFree = true;
};