// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallGameMode.h"

#include "NightfallCharacter.h"
#include "NightfallPlayerController.h"

ANightfallGameMode::ANightfallGameMode()
{
	DefaultPawnClass = ANightfallCharacter::StaticClass();
	PlayerControllerClass = ANightfallPlayerController::StaticClass();

	// The slice is single player and starts immediately; no seamless travel, no lobby.
	bStartPlayersAsSpectators = false;
	bUseSeamlessTravel = false;
}
