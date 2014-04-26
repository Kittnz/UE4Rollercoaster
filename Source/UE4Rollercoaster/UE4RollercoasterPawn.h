// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/DefaultPawn.h"
#include "UE4RollercoasterPawn.generated.h"

/**
 * 
 */
UCLASS()
class AUE4RollercoasterPawn : public ADefaultPawn
{
	GENERATED_UCLASS_BODY()

public:
	virtual FRotator GetViewRotation() const OVERRIDE;

};
