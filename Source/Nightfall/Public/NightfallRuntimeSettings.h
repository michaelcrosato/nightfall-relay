// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NightfallRuntimeSettings.generated.h"

class UNightfallInputConfig;

/** Unit the HUD reads surface temperature out in. */
UENUM()
enum class ENightfallTemperatureUnit : uint8
{
	Celsius		UMETA(DisplayName = "Celsius"),
	Fahrenheit	UMETA(DisplayName = "Fahrenheit")
};

/**
 * Project-wide tuning that is not per-actor. Lives in Config/DefaultGame.ini and is
 * editable under Project Settings > Game > Nightfall Runtime.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Nightfall Runtime"))
class NIGHTFALL_API UNightfallRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNightfallRuntimeSettings();

	static const UNightfallRuntimeSettings& Get();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Real-world minutes for one complete 24 hour in-game cycle. */
	UPROPERTY(config, EditAnywhere, Category = "Day Night", meta = (ClampMin = "0.5", UIMin = "1.0", UIMax = "60.0"))
	float DayLengthMinutes;

	/**
	 * In-game hour the slice starts at. The sun crosses the horizon at exactly 18.0 and the
	 * night key is reached at 18.5, so 17.85 opens at dusk: sun a degree above the horizon,
	 * light shafts still raking, and the first pylon already worth lighting.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Day Night", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float StartTimeOfDayHours;

	/** Compass bearing the sun rises from, degrees. */
	UPROPERTY(config, EditAnywhere, Category = "Day Night", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float SunriseAzimuthDegrees;

	/**
	 * Peak solar altitude at local noon. Kept well below 90 on purpose: a low sun keeps
	 * shadows long and raking, which is where the whole look of this project lives.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Day Night", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float MaxSunAltitudeDegrees;

	/**
	 * Calendar date the slice opens on, as YYYY-MM-DD. The clock counts days from here, so
	 * the HUD can show a date that advances rather than a bare hour. Anything unparseable
	 * falls back to the class default and is logged.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Day Night")
	FString StartDate;

	/**
	 * Surface temperature the field settles at after a full night with no sun on it. The
	 * floor rather than the minimum: the model approaches it asymptotically, so the coldest
	 * reading of the cycle lands in the hour before dawn.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Environment", meta = (UIMin = "-40.0", UIMax = "40.0"))
	float NightTemperatureCelsius;

	/** Temperature the field settles at under a sun that has been high for hours. */
	UPROPERTY(config, EditAnywhere, Category = "Environment", meta = (UIMin = "-20.0", UIMax = "60.0"))
	float DayTemperatureCelsius;

	/**
	 * How long the ground takes to answer a change in sunlight, in in-game hours. This is
	 * the whole reason the temperature curve is not simply the sun's altitude: it puts the
	 * warmest hour in the afternoon and the coldest just before dawn.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Environment", meta = (ClampMin = "0.25", UIMax = "8.0"))
	float ThermalLagHours;

	/** Unit the HUD reads the temperature out in. The model itself is always Celsius. */
	UPROPERTY(config, EditAnywhere, Category = "Environment")
	ENightfallTemperatureUnit TemperatureUnit;

	/** Distance in cm the interaction trace reaches from the camera. */
	UPROPERTY(config, EditAnywhere, Category = "Interaction", meta = (ClampMin = "50.0"))
	float InteractionTraceDistance;

	/** Radius in cm of the interaction sweep, which makes small targets forgiving. */
	UPROPERTY(config, EditAnywhere, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractionTraceRadius;

	/** Frames per second the slice is expected to hold. The perf HUD colours against this. */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "20.0"))
	float TargetFrameRate;

	/**
	 * A frame costing more than this many milliseconds over the target budget is recorded
	 * as a hitch and attributed to streaming if cells were in flight at the time.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1.0"))
	float HitchThresholdMilliseconds;

	/** Save slot used by UNightfallSaveSubsystem. */
	UPROPERTY(config, EditAnywhere, Category = "Save")
	FString SaveSlotName;

	/**
	 * Input config the player character loads when it has no per-instance override. Kept
	 * here rather than on a Blueprint default so the whole project stays authored in C++
	 * and generated assets.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (AllowedClasses = "/Script/Nightfall.NightfallInputConfig"))
	TSoftObjectPtr<UNightfallInputConfig> DefaultInputConfig;

	/**
	 * Show the briefing card when the game starts. The clock is held while it is up, so
	 * turning this off also means the day begins running from the first frame.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	bool bShowBriefingOnStart;

	/**
	 * Footprint about the origin that reads as levelled, surfaced ground and never raises
	 * dust. Half extents in cm. The filament beds claim their own footprints at BeginPlay;
	 * this covers the compound, which owns no actor able to speak for itself.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dust")
	FVector2D CompoundCleanHalfExtent;
};
