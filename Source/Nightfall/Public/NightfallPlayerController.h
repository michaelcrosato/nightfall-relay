// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NightfallPlayerController.generated.h"

/**
 * Player controller for the slice.
 *
 * Deliberately thin. Gameplay input lives on the character and UI input is bound by
 * UNightfallUISubsystem, so this class only owns what genuinely belongs to the controller:
 * the input mode, and the camera manager's pitch limits.
 */
UCLASS()
class NIGHTFALL_API ANightfallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANightfallPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Restore game-only input after a menu closes. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Input")
	void ApplyGameInputMode();

	/** Switch to menu input: cursor shown, game input released. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Input")
	void ApplyMenuInputMode();
};
