// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/SNightfallPerfHud.h"
#include "NightfallUISubsystem.generated.h"

class APlayerController;
class SNightfallBriefing;
class SNightfallHudRoot;
class SNightfallSettingsMenu;
class SWidget;
class UInputComponent;
class UNightfallWorldClockSubsystem;

/**
 * Owns the interface for one local player.
 *
 * Creates the HUD, opens and closes the settings menu, cycles the performance panel, and
 * offers the only extension point anything else needs: RegisterHudPanel. A Game Feature
 * Plugin calls that with its own Slate widget and a layer tag, and unregisters on
 * deactivate. Panels registered before the HUD exists are queued and attached when it does,
 * so a feature that activates during startup does not have to care about ordering.
 */
UCLASS()
class NIGHTFALL_API UNightfallUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The subsystem for the first local player, or null before one exists. */
	static UNightfallUISubsystem* Get(const UObject* WorldContextObject);

	/** Build the HUD and put it on the viewport. Called by ANightfallPlayerController. */
	void CreateHud(APlayerController* OwningController);

	/** Add the menu and performance HUD bindings to the controller's input component. */
	void BindInputActions(UInputComponent* InputComponent);

	/** Put a widget into a named HUD layer. Safe to call before the HUD is built. */
	bool RegisterHudPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel);

	/** Remove a widget previously registered into a layer. */
	void UnregisterHudPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel);

	UFUNCTION(BlueprintCallable, Category = "Nightfall|UI")
	void ToggleSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "Nightfall|UI")
	void CloseSettingsMenu();

	UFUNCTION(BlueprintPure, Category = "Nightfall|UI")
	bool IsSettingsMenuOpen() const { return SettingsMenu.IsValid(); }

	/** Step the performance HUD through hidden, compact and full. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|UI")
	void CyclePerfHud();

	/**
	 * Put the briefing card on screen and hold the day clock behind it. Called by
	 * ANightfallPlayerController at startup, and by the Nightfall.Briefing command.
	 */
	void ShowBriefing();

	/** Take the card down and let the day run again. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|UI")
	void DismissBriefing();

	UFUNCTION(BlueprintCallable, Category = "Nightfall|UI")
	void ToggleBriefing();

	UFUNCTION(BlueprintPure, Category = "Nightfall|UI")
	bool IsBriefingOpen() const { return Briefing.IsValid(); }

private:
	void OpenSettingsMenu();

	/** The world clock for this player's world, or null before one exists. */
	UNightfallWorldClockSubsystem* GetClock() const;

	TSharedPtr<SNightfallHudRoot> HudRoot;
	TSharedPtr<SNightfallSettingsMenu> SettingsMenu;
	TSharedPtr<SNightfallBriefing> Briefing;

	/**
	 * True when showing the briefing is what stopped the clock, so dismissing it only
	 * restarts a day this subsystem paused - not one the player paused from the console.
	 */
	bool bPausedClockForBriefing = false;

	TWeakObjectPtr<APlayerController> WeakController;

	/** Panels registered before the HUD existed, replayed once it does. */
	TArray<TPair<FGameplayTag, TSharedRef<SWidget>>> PendingPanels;

	/**
	 * Performance HUD mode asked for before the HUD existed, applied once it does. Startup
	 * console commands run before the player controller has built the HUD, so without this
	 * a mode set from the command line is silently dropped.
	 */
	TOptional<ENightfallPerfHudMode> PendingPerfHudMode;

	/** Console objects registered by this subsystem, released on shutdown. */
	TArray<IConsoleObject*> ConsoleObjects;
};
