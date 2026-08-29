// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightfallWorldClockSubsystem.generated.h"

UENUM(BlueprintType)
enum class ENightfallTimePhase : uint8
{
	Night	UMETA(DisplayName = "Night"),
	Dawn	UMETA(DisplayName = "Dawn"),
	Day		UMETA(DisplayName = "Day"),
	Dusk	UMETA(DisplayName = "Dusk")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightfallTimePhaseChanged, ENightfallTimePhase, NewPhase);

/**
 * Authoritative clock for the slice.
 *
 * Owns nothing renderable. It converts elapsed real time into a calendar date, an in-game
 * hour, a solar direction and the surface temperature that direction implies;
 * ANightfallSkyDirector reads the solar part and drives the actual lights. Keeping the two
 * apart means a Game Feature Plugin can read or scrub time without touching lighting code.
 *
 * The solar model is a tilted great circle rather than a real ephemeris: peak altitude is
 * clamped to MaxSunAltitudeDegrees so noon still rakes rather than flattening the scene.
 */
UCLASS()
class NIGHTFALL_API UNightfallWorldClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End USubsystem

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** Current in-game hour in the range [0, 24). */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	/** Scrub the clock. Values outside the day range wrap. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Time")
	void SetTimeOfDayHours(float NewHours);

	/** Advance, or with a negative value rewind, the clock by a number of in-game hours. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Time")
	void AdvanceHours(float DeltaHours);

	/** Real-world minutes per full 24 hour cycle. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	float GetDayLengthMinutes() const { return DayLengthMinutes; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Time")
	void SetDayLengthMinutes(float NewLength);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	bool IsPaused() const { return bPaused; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Time")
	void SetPaused(bool bNewPaused) { bPaused = bNewPaused; }

	/** Unit vector pointing from the world toward the sun. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FVector GetSunDirection() const { return SunDirection; }

	/** Rotation for a directional light representing the sun. Light travels along +X. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FRotator GetSunRotation() const { return (-SunDirection).Rotation(); }

	/** Rotation for the moon, which sits opposite the sun. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FRotator GetMoonRotation() const { return SunDirection.Rotation(); }

	/** Solar altitude in degrees. Negative below the horizon. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	float GetSunAltitudeDegrees() const { return SunAltitudeDegrees; }

	/** True between local midnight and local noon, used to pick dawn versus dusk grading. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	bool IsMorning() const { return TimeOfDayHours < 12.0f; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	ENightfallTimePhase GetTimePhase() const { return CurrentPhase; }

	/** Whole days elapsed since the slice opened. Zero on the first day. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	int32 GetDayIndex() const { return DayIndex; }

	/** Jump to a day. Negative values are clamped away; the hour is left where it is. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Time")
	void SetDayIndex(int32 NewDayIndex);

	/** Calendar date and hour, the start date advanced by the days and hours elapsed. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FDateTime GetDateTime() const;

	/** Clock string in 24 hour form for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FString GetClockString() const;

	/** Date string for the HUD, as 04 NOV 2231. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Time")
	FString GetDateString() const;

	/**
	 * Surface temperature at the current hour.
	 *
	 * Not a reading off the sun: the ground answers sunlight slowly, so this is an
	 * exponentially weighted average of the solar load over the hours behind it. That lag
	 * is what puts the warmest hour of the cycle in the afternoon and the coldest in the
	 * hour before dawn, rather than at noon and midnight.
	 */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Environment")
	float GetTemperatureCelsius() const { return TemperatureCelsius; }

	/** The same value in the unit the project is configured to read out in. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Environment")
	float GetTemperatureInDisplayUnit() const;

	/** Temperature string for the HUD, unit suffix included. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Environment")
	FString GetTemperatureString() const;

	/** Fires when the clock crosses into a new phase. */
	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Time")
	FNightfallTimePhaseChanged OnTimePhaseChanged;

private:
	void RecomputeSolarState();
	void UpdatePhaseAndBroadcast();
	static ENightfallTimePhase ClassifyPhase(float AltitudeDegrees, bool bMorning);

	/** Solar altitude at an arbitrary hour, in degrees. The closed form of the arc above. */
	float SunAltitudeDegreesAtHour(float Hours) const;

	/** Fraction of full sun landing on the ground at an hour, in the range [0, 1]. */
	float SolarLoadAtHour(float Hours) const;

	/** Settle the temperature to whatever the hours behind the current one imply. */
	void RecomputeTemperature();

	/** In-game hour in the range [0, 24). */
	float TimeOfDayHours = 17.5f;

	/** Whole days elapsed since the slice opened. */
	int32 DayIndex = 0;

	/** Real-world minutes per full cycle. */
	float DayLengthMinutes = 12.0f;

	float SunriseAzimuthDegrees = -75.0f;
	float MaxSunAltitudeDegrees = 52.0f;

	FVector SunDirection = FVector::UpVector;
	float SunAltitudeDegrees = 0.0f;

	/** Midnight on the day the slice opens, parsed once from the runtime settings. */
	FDateTime StartDate = FDateTime(2231, 11, 4);

	float NightTemperatureCelsius = -6.0f;
	float DayTemperatureCelsius = 14.0f;
	float ThermalLagHours = 3.0f;

	float TemperatureCelsius = 0.0f;

	ENightfallTimePhase CurrentPhase = ENightfallTimePhase::Day;
	bool bPaused = false;

	/** Console objects registered by this subsystem, released on shutdown. */
	TArray<IConsoleObject*> ConsoleObjects;
};
