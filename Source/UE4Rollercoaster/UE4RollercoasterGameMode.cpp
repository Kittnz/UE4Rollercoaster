// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UE4Rollercoaster.h"
#include "UE4RollercoasterGameMode.h"
#include "UE4RollercoasterPlayerController.h"
#include "UE4RollercoasterPawn.h"

AUE4RollercoasterGameMode::AUE4RollercoasterGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DefaultPawnClass = AUE4RollercoasterPawn::StaticClass();
	PlayerControllerClass = AUE4RollercoasterPlayerController::StaticClass();
}


