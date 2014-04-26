// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UE4Rollercoaster.h"
#include "UE4RollercoasterPlayerController.h"
#include "IHeadMountedDisplay.h"

AUE4RollercoasterPlayerController::AUE4RollercoasterPlayerController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	CurentSegmentIdx = 0;
	CurrentSegmentDelta = 0;
	CurrentSegmentLength = 0;
	CurrentRollerCoasterVelocity = 30.f;
	RollerCoasterVelocity = 30.f;
}

static float ApproxLength(const FInterpCurveVector& SplineInfo, const float Start = 0.0f, const float End = 1.0f, const int32 ApproxSections = 4)
{
	float SplineLength = 0;
	FVector OldPos = SplineInfo.Eval(Start, FVector::ZeroVector);
	for (int32 i = 1; i <= ApproxSections; i++)
	{
		FVector NewPos = SplineInfo.Eval(FMath::Lerp(Start, End, (float)i / (float)ApproxSections), FVector::ZeroVector);
		SplineLength += (NewPos - OldPos).Size();
		OldPos = NewPos;
	}

	return SplineLength;
}

/* PlayerTick is only called if the PlayerController has a PlayerInput object.  Therefore, it will not be called on servers for non-locally controlled playercontrollers. */
void AUE4RollercoasterPlayerController::PlayerTick(float DeltaTime)
{
	// HACK: This is slow, search all objects in the scene to find a spline component to ride upon
	if (!TrackSplines && GetPawn())
	{
		for (TObjectIterator<ULandscapeSplinesComponent> ObjIt; ObjIt; ++ObjIt)
		{
			TrackSplines = *ObjIt;

			float Dummy;
			FVector Dummy2;
			FVector NearestLocation;
			FVector PlayerStartLocation = GetPawn()->GetActorLocation() - TrackSplines->GetOwner()->GetActorLocation();
			for (TObjectIterator<APlayerStart> StartIt; StartIt; ++StartIt)
			{
				PlayerStartLocation = StartIt->GetActorLocation() - TrackSplines->GetOwner()->GetActorLocation();
				break;
			}

			//Find the segment closest to the player start point
			float ClosestDistSq = FLT_MAX;
			for (int32 i = 0; i < TrackSplines->Segments.Num(); i++)
			{
				TrackSplines->Segments[i]->FindNearest(PlayerStartLocation, Dummy, NearestLocation, Dummy2);
				float DistSq = (NearestLocation - PlayerStartLocation).SizeSquared();
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					CurentSegmentIdx = i;
				}
			}

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = TrackSplines->Segments[CurentSegmentIdx]->GetSplineInfo();
			CurrentSegmentLength = ApproxLength(SplineInfo, 0.0f, 1.0f, 4);

			break;
		}
	}

	if (GetPawn() && TrackSplines)
	{
		CurrentSegmentDelta += CurrentRollerCoasterVelocity*DeltaTime;

		//If we're going to pass the next point this frame
		if (CurrentSegmentDelta > CurrentSegmentLength)
		{
			CurentSegmentIdx = CurentSegmentIdx + 1 >= TrackSplines->Segments.Num() ? 0 : CurentSegmentIdx + 1;
			CurrentSegmentDelta = 0.f;

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = TrackSplines->Segments[CurentSegmentIdx]->GetSplineInfo();
			CurrentSegmentLength = ApproxLength(SplineInfo, 0.0f, 1.0f, 4);
		}

		ULandscapeSplineSegment* CurrentSegment = TrackSplines->Segments[CurentSegmentIdx];
		const FInterpCurveVector& SplineInfo = CurrentSegment->GetSplineInfo();

		//Do some moving along the track!
		const float NewKeyTime = CurrentSegmentDelta / CurrentSegmentLength;
		const FVector NewKeyPos = TrackSplines->GetOwner()->GetActorLocation() + SplineInfo.Eval(NewKeyTime, FVector::ZeroVector);
		const FVector NewKeyTangent = SplineInfo.EvalDerivative(NewKeyTime, FVector::ZeroVector).SafeNormal();
		FRotator NewRotation = NewKeyTangent.Rotation();

		// Calculate the roll values
		float NewRotationRoll = 0.f;
		if (CurrentSegment->Connections[0].ControlPoint && CurrentSegment->Connections[1].ControlPoint)
		{
			FVector StartLocation; FRotator StartRotation;
			CurrentSegment->Connections[0].ControlPoint->GetConnectionLocationAndRotation(CurrentSegment->Connections[0].SocketName, StartLocation, StartRotation);
			FVector EndLocation; FRotator EndRotation;
			CurrentSegment->Connections[1].ControlPoint->GetConnectionLocationAndRotation(CurrentSegment->Connections[1].SocketName, EndLocation, EndRotation);
			NewRotationRoll = FMath::Lerp(-StartRotation.Roll, -EndRotation.Roll, NewKeyTime);
		}
		NewRotation.Roll = NewRotationRoll;

		//Make the controller/camera face the right direction
		ChairViewRotation = NewRotation;
		if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
		{
			//Remove the Pitch and Roll for VR
			ChairViewRotation.Pitch = 0;
			ChairViewRotation.Roll = 0;
		}
		SetControlRotation(ChairViewRotation);

		//Make the pawn/chair move along the track
		GetPawn()->SetActorLocation(NewKeyPos);
		GetPawn()->SetActorRotation(NewRotation);

		//Offset the camera so it's above the seat
		CameraOffset = FRotationMatrix(NewRotation).GetScaledAxis(EAxis::Z) * 5.f;

		//Adjust the velocity of the coaster. Increase acceleration/deceleration when on a slope
		const float Acceleration = FMath::Lerp(20.f, 49.f, FMath::Abs(NewKeyTangent.Z));
		const float VelocityDiff = RollerCoasterVelocity - CurrentRollerCoasterVelocity;
		CurrentRollerCoasterVelocity += FMath::Min(Acceleration, FMath::Abs(VelocityDiff)) * DeltaTime * (VelocityDiff > 0 ? 1.f : -1.f);
	}

	PlayerCameraManager->bFollowHmdOrientation = true;

	Super::PlayerTick(DeltaTime);
}

/**
* Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
* This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
*/
void AUE4RollercoasterPlayerController::UpdateRotation(float DeltaTime)
{
	Super::UpdateRotation(DeltaTime);
}

void AUE4RollercoasterPlayerController::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
	Super::GetPlayerViewPoint(Location, Rotation);
	Location += CameraOffset;

	Rotation = ChairViewRotation;
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		// Disable bUpdateOnRT if using this method.
		FQuat HMDOrientation;
		FVector HMDPosition;
		GEngine->HMDDevice->GetCurrentOrientationAndPosition(HMDOrientation, HMDPosition);

		// Only keep the yaw component from the chair.
		Rotation = HMDOrientation.Rotator();
		Rotation.Yaw += ChairViewRotation.Yaw;
	}
}