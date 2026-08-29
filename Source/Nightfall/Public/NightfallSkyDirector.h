// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightfallSkyDirector.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UNightfallSkyProfile;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

/**
 * The single actor that owns the sky.
 *
 * It reads the hour from UNightfallWorldClockSubsystem, asks UNightfallSkyProfile what that
 * hour should look like, and pushes the result into the sun, the moon, the sky light, the
 * height fog and an unbound post process volume. One actor, one place to look when the
 * lighting is wrong.
 *
 * Deliberately not a Blueprint: every value it writes comes from the profile asset, so
 * there is nothing here a designer would want to rewire in a graph.
 */
UCLASS(NotBlueprintable)
class NIGHTFALL_API ANightfallSkyDirector : public AActor
{
	GENERATED_BODY()

public:
	ANightfallSkyDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** The authored look. Assigned in the level; falls back to the class default object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Sky")
	TObjectPtr<UNightfallSkyProfile> SkyProfile;

	/** Solar altitude at or above which the sun delivers its full authored lux. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Sky")
	float SunFadeStartAltitudeDegrees = 2.0f;

	/** Solar altitude at or below which the sun lights nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Sky")
	float SunFadeEndAltitudeDegrees = -2.0f;

	/**
	 * Lux the sun keeps once it has faded out completely. Must stay above zero: the engine
	 * drops a light of zero intensity from the scene exactly as if it were hidden, which
	 * would hand the sky atmosphere's sun slot to the moon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Sky")
	float SunBelowHorizonFloorLux = 0.001f;

	/**
	 * Exposure range in EV100, not luminance: the project runs with the extended default
	 * luminance range so that physical light units and exposure speak the same language.
	 *
	 * The range has to span the whole cycle. Roughly EV 15 at noon under a 75000 lux sun
	 * down to EV -3 at night lit only by moonlight and emissive panels; clamping tighter
	 * than that makes one end of the day clip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Exposure")
	float AutoExposureMinBrightness = -2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Exposure")
	float AutoExposureMaxBrightness = 16.0f;

	/** Eye adaptation speed when the view gets brighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Exposure")
	float AutoExposureSpeedUp = 2.6f;

	/** Eye adaptation speed when the view gets darker. Slower, so stepping into shadow reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Exposure")
	float AutoExposureSpeedDown = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Bloom")
	float BloomIntensity = 0.55f;

	/** Distance in cm volumetric fog is computed out to. Directly proportional to its cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Fog")
	float VolumetricFogDistance = 22000.0f;

	/** Push the current profile state into every component immediately. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Sky")
	void ApplySkyState(float SunAltitudeDegrees, bool bMorning);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Sky")
	UDirectionalLightComponent* GetSunLight() const { return SunLight; }

private:
	/** Resolve the profile to use, falling back to the class default when none is assigned. */
	const UNightfallSkyProfile* ResolveProfile() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> PostProcess;
};
