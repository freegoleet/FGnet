#pragma once

#include "Engine/DataAsset.h"
#include "FGPlayerSettings.generated.h"


UCLASS()
class FGNET_API UFGPlayerSettings : public UDataAsset
{
	GENERATED_BODY()

public:	
	UPROPERTY(EditAnywhere, category = "Movement")
	float Acceleration = 500.0f;

	UPROPERTY(EditAnywhere, category = "Movement", meta = (DisplayName = "TurnSpeed"))
	float TurnSpeedDefault = 100.0f;

	UPROPERTY(EditAnywhere, category = "Movement")
	float MaxVelocity = 2000.0f;

	UPROPERTY(EditAnywhere, category = "Movement", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DefaultFriction = 0.75f;

	UPROPERTY(EditAnywhere, category = "Movement", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float BrakingFriction = 0.001f;

	UPROPERTY(EditAnywhere, Category = "Fire", meta = (ClampMin = 0.0))
	float FireCooldown = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = 1))
	int32 StartingHealth = 100;

};
