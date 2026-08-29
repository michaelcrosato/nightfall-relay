// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "NightfallRuntimeSettings.h"
#include "UI/NightfallUISubsystem.h"

ANightfallPlayerController::ANightfallPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ANightfallPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerCameraManager)
	{
		// Stop just short of vertical so the horizon never inverts.
		PlayerCameraManager->ViewPitchMin = -87.0f;
		PlayerCameraManager->ViewPitchMax = 87.0f;
	}

	ApplyGameInputMode();

	// The UI subsystem owns every widget; the controller only tells it when to appear.
	if (UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this))
	{
		// CreateHud first: it is what gives the subsystem the controller the briefing needs
		// to reach the world clock.
		UI->CreateHud(this);

		if (UNightfallRuntimeSettings::Get().bShowBriefingOnStart)
		{
			UI->ShowBriefing();
		}
	}
}

void ANightfallPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Menu and performance HUD bindings live with the UI, not with gameplay input.
	if (UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this))
	{
		UI->BindInputActions(InputComponent);
	}
}

void ANightfallPlayerController::ApplyGameInputMode()
{
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void ANightfallPlayerController::ApplyMenuInputMode()
{
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}
