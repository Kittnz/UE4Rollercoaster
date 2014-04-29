// Teddy0@gmail.com

#include "UE4Rollercoaster.h"
#include "UE4RollercoasterPlayerController.h"
#include "IHeadMountedDisplay.h"

//Spline helper functions
static float ApproxLength(const FInterpCurveVector& SplineInfo, const float Start = 0.0f, const float End = 1.0f, const int32 ApproxSections = 32)
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

static float GetKeyForDistance(const FInterpCurveVector& SplineInfo, const float Distance, const int32 ApproxSections = 32)
{
	float SplineLength = 0;
	FVector OldPos = SplineInfo.Eval(0.0f, FVector::ZeroVector);
	for (int32 i = 1; i <= ApproxSections; i++)
	{
		FVector NewPos = SplineInfo.Eval((float)i / (float)ApproxSections, FVector::ZeroVector);
		float SectionLength = (NewPos - OldPos).Size();

		if (SplineLength + SectionLength >= Distance)
		{
			float DistanceAlongSection = Distance - SplineLength;
			float SectionDelta = DistanceAlongSection / SectionLength;
			return i / (float)ApproxSections - (1.f - SectionDelta) / (float)ApproxSections;
		}
		SplineLength += SectionLength;
		OldPos = NewPos;
	}

	return 1.0f;
}

//Player Controller class that moves along a Landscape Spline
AUE4RollercoasterPlayerController::AUE4RollercoasterPlayerController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	CurentSegmentIdx = 0;
	CurrentSegmentDelta = 0;
	CurrentSegmentLength = 0;
	CurrentRollerCoasterVelocity = 30.f;
	RollerCoasterVelocity = 30.f;
	ConfigCameraPitch = false;
	BlueprintCameraPitch = false;
	ConfigCameraRoll = false;
	BlueprintCameraRoll = false;
}

void AUE4RollercoasterPlayerController::Possess(APawn* PawnToPossess)
{
	Super::Possess(PawnToPossess);

	//Remove Pitch/Roll limits, so we can do loop-de-loops
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = 0.f;
		PlayerCameraManager->ViewPitchMax = 359.999f;
		PlayerCameraManager->ViewYawMin = 0.f;
		PlayerCameraManager->ViewYawMax = 359.999f;
		PlayerCameraManager->ViewRollMin = 0.f;
		PlayerCameraManager->ViewRollMax = 359.999f;
	}

	if (GetPawn())
	{
		for (TObjectIterator<ULandscapeSplinesComponent> ObjIt; ObjIt; ++ObjIt)
		{
			TrackSplines = *ObjIt;

			ULandscapeSplineControlPoint* ClosestControlPoint = nullptr;

			//Find the controlpoint closest to the player start point
			FVector PlayerStartLocation = GetPawn()->GetActorLocation() - TrackSplines->GetOwner()->GetActorLocation();
			float ClosestDistSq = FLT_MAX;
			for (int32 i = 0; i < TrackSplines->ControlPoints.Num(); i++)
			{
				float DistSq = (TrackSplines->ControlPoints[i]->Location - PlayerStartLocation).SizeSquared();
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestControlPoint = TrackSplines->ControlPoints[i];
				}
			}

			//Build up an ordered list of track segments (the TrackSplines->Segments array can be in random order)
			bool bTrackHasErrors = false;
			OrderedSegments.Empty(TrackSplines->Segments.Num());
			ULandscapeSplineControlPoint* CurrentControlPoint = ClosestControlPoint;
			while (CurrentControlPoint)
			{
				int32 i = 0;
				for (; i < CurrentControlPoint->ConnectedSegments.Num(); i++)
				{
					if (OrderedSegments.Num() == 0 || CurrentControlPoint->ConnectedSegments[i].Segment != OrderedSegments[0])
					{
						OrderedSegments.Insert(CurrentControlPoint->ConnectedSegments[i].Segment, 0);
						check(CurrentControlPoint != CurrentControlPoint->ConnectedSegments[i].GetFarConnection().ControlPoint);
						CurrentControlPoint = CurrentControlPoint->ConnectedSegments[i].GetFarConnection().ControlPoint;
						break;
					}
				}

				//We didn't find another segment to link to, we have an error!
				if (i == CurrentControlPoint->ConnectedSegments.Num())
				{
					bTrackHasErrors = true;
					break;
				}

				//Back to the start
				if (CurrentControlPoint == ClosestControlPoint)
					break;
			}

			//If we found any segments that weren't linked, we have an error
			if (OrderedSegments.Num() != TrackSplines->Segments.Num())
				bTrackHasErrors = true;

			//If there are any errors, bail out!
			if (bTrackHasErrors)
			{
				TrackSplines = nullptr;
				OrderedSegments.Empty();
				continue;
			}

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = OrderedSegments[0]->GetSplineInfo();
			CurrentSegmentLength = ApproxLength(SplineInfo);

			break;
		}
	}
}

void AUE4RollercoasterPlayerController::UnPossess()
{
	Super::UnPossess();

	TrackSplines = nullptr;
	OrderedSegments.Empty();
}

/* PlayerTick is only called if the PlayerController has a PlayerInput object.  Therefore, it will not be called on servers for non-locally controlled playercontrollers. */
void AUE4RollercoasterPlayerController::PlayerTick(float DeltaTime)
{
	//If we're requesting a stop, do it immediately
	if (RollerCoasterVelocity == 0.f)
		CurrentRollerCoasterVelocity = 0.f;

	if (GetPawn() && TrackSplines)
	{
		CurrentSegmentDelta += CurrentRollerCoasterVelocity*DeltaTime;

		//If we're going to pass the next point this frame
		if (CurrentSegmentDelta > CurrentSegmentLength)
		{
			CurentSegmentIdx = CurentSegmentIdx + 1 >= OrderedSegments.Num() ? 0 : CurentSegmentIdx + 1;
			CurrentSegmentDelta = 0.f;

			//Calculate the length of this point
			const FInterpCurveVector& SplineInfo = OrderedSegments[CurentSegmentIdx]->GetSplineInfo();
			CurrentSegmentLength = ApproxLength(SplineInfo);
		}

		ULandscapeSplineSegment* CurrentSegment = OrderedSegments[CurentSegmentIdx];
		const FInterpCurveVector& SplineInfo = CurrentSegment->GetSplineInfo();

		//Do some moving along the track!
		const float NewKeyTime = GetKeyForDistance(SplineInfo, CurrentSegmentDelta);
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
			if (!ConfigCameraPitch && !BlueprintCameraRoll)
				ChairViewRotation.Pitch = 0;
			if (!ConfigCameraRoll && !BlueprintCameraRoll)
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
}