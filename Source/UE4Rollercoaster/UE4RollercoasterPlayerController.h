// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "UE4RollercoasterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class AUE4RollercoasterPlayerController : public APlayerController
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FRotator ChairViewRotation;

	UPROPERTY()
	class ULandscapeSplinesComponent* TrackSplines;

	UPROPERTY()
	int32 CurentSegmentIdx;

	UPROPERTY()
	float CurrentSegmentDelta;

	UPROPERTY()
	float CurrentSegmentLength;

	UPROPERTY()
	FVector CameraOffset;

	UPROPERTY()
	float CurrentRollerCoasterVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	float RollerCoasterVelocity;

public:
	/**
	* Processes player input (immediately after PlayerInput gets ticked) and calls UpdateRotation().
	* PlayerTick is only called if the PlayerController has a PlayerInput object. Therefore, it will only be called for locally controlled PlayerControllers.
	*/
	virtual void PlayerTick(float DeltaTime);

	/**
	* Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
	* This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
	*/
	virtual void UpdateRotation(float DeltaTime) OVERRIDE;

	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const OVERRIDE;
};
