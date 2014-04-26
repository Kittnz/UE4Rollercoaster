// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UE4Rollercoaster.h"
#include "UE4RollercoasterPawn.h"
#include "UE4RollercoasterPlayerController.h"


AUE4RollercoasterPawn::AUE4RollercoasterPawn(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	
}

FRotator AUE4RollercoasterPawn::GetViewRotation() const
{
	return Super::GetViewRotation();
}