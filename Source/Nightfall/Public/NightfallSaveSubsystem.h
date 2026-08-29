// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NightfallSaveSubsystem.generated.h"

class UNightfallSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightfallSaveEvent, bool, bSuccess);

/**
 * Save and load for the slice.
 *
 * Walks every actor implementing INightfallSaveable and serialises the properties it and
 * its components mark SaveGame, plus the world clock and where the player was standing.
 * Nothing here knows what any particular system stores, which is the point: a Game Feature
 * Plugin persists state by marking a property on its component and implementing nothing.
 *
 * Console commands Nightfall.Save and Nightfall.Load drive it during play.
 */
UCLASS()
class NIGHTFALL_API UNightfallSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UNightfallSaveSubsystem* Get(const UObject* WorldContextObject);

	/** Write the current world to the configured slot. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Save")
	bool SaveGame();

	/** Restore the configured slot into the current world. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Save")
	bool LoadGame();

	UFUNCTION(BlueprintPure, Category = "Nightfall|Save")
	bool HasSave() const;

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Save")
	bool DeleteSave();

	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Save")
	FNightfallSaveEvent OnSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Save")
	FNightfallSaveEvent OnLoadCompleted;

private:
	FString GetSlotName() const;

	void CaptureActors(UNightfallSaveGame& Save) const;
	void RestoreActors(const UNightfallSaveGame& Save) const;

	/** Console command handles, unregistered on shutdown. */
	TArray<IConsoleObject*> ConsoleCommands;
};
