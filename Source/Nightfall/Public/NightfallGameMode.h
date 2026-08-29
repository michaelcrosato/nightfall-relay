// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NightfallGameMode.generated.h"

/**
 * Wires the default classes for the slice.
 *
 * There is no win or lose state here on purpose. The gameplay loop lives entirely in the
 * GridRestoration Game Feature Plugin, which is the whole point of the architecture: the
 * game mode should not have to change when the game does.
 */
UCLASS()
class NIGHTFALL_API ANightfallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANightfallGameMode();
};
