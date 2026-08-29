// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallSkyDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "NightfallGameUserSettings.h"
#include "NightfallSkyProfile.h"
#include "NightfallStats.h"
#include "NightfallWorldClockSubsystem.h"

ANightfallSkyDirector::ANightfallSkyDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(SceneRoot);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->bAtmosphereSunLight = true;
	SunLight->AtmosphereSunLightIndex = 0;
	SunLight->bCastShadowsOnAtmosphere = true;
	SunLight->bEnableLightShaftOcclusion = true;
	SunLight->CastShadows = true;
	SunLight->bCastVolumetricShadow = true;
	// Two atmosphere lights means the renderer has to be told which one owns forward
	// shading, translucency and volumetric fog. The sun does.
	SunLight->ForwardShadingPriority = 100;
	SunLight->Intensity = 75000.0f;

	// The moon is a second atmosphere light rather than a faked ambient lift, so it
	// scatters through the same sky and casts the same virtual shadow maps.
	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(SceneRoot);
	MoonLight->SetMobility(EComponentMobility::Movable);
	MoonLight->bAtmosphereSunLight = true;
	MoonLight->AtmosphereSunLightIndex = 1;
	MoonLight->bCastShadowsOnAtmosphere = false;
	MoonLight->CastShadows = true;
	MoonLight->bCastVolumetricShadow = true;
	MoonLight->ForwardShadingPriority = 0;
	MoonLight->Intensity = 0.38f;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(SceneRoot);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = SLS_CapturedScene;
	SkyLight->bRealTimeCapture = true;
	SkyLight->bLowerHemisphereIsBlack = false;
	SkyLight->Intensity = 1.0f;

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(SceneRoot);
	HeightFog->bEnableVolumetricFog = true;
	HeightFog->VolumetricFogDistance = 22000.0f;
	HeightFog->FogDensity = 0.028f;
	HeightFog->FogHeightFalloff = 0.18f;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 0.0f;
	PostProcess->BlendWeight = 1.0f;
}

void ANightfallSkyDirector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Give the editor viewport the authored dusk state without needing to press play.
	if (const UNightfallSkyProfile* Profile = ResolveProfile())
	{
		ApplySkyState(6.0f, /*bMorning=*/false);
	}
}

void ANightfallSkyDirector::BeginPlay()
{
	Super::BeginPlay();

	if (const UNightfallWorldClockSubsystem* Clock = GetWorld()->GetSubsystem<UNightfallWorldClockSubsystem>())
	{
		ApplySkyState(Clock->GetSunAltitudeDegrees(), Clock->IsMorning());
		SetActorRotation(FRotator::ZeroRotator);
		SunLight->SetWorldRotation(Clock->GetSunRotation());
		MoonLight->SetWorldRotation(Clock->GetMoonRotation());
	}
}

void ANightfallSkyDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_SkyDirector);

	const UNightfallWorldClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UNightfallWorldClockSubsystem>() : nullptr;
	if (!Clock)
	{
		return;
	}

	SunLight->SetWorldRotation(Clock->GetSunRotation());
	MoonLight->SetWorldRotation(Clock->GetMoonRotation());

	ApplySkyState(Clock->GetSunAltitudeDegrees(), Clock->IsMorning());
}

const UNightfallSkyProfile* ANightfallSkyDirector::ResolveProfile() const
{
	return SkyProfile ? SkyProfile.Get() : GetDefault<UNightfallSkyProfile>();
}

void ANightfallSkyDirector::ApplySkyState(float SunAltitudeDegrees, bool bMorning)
{
	const UNightfallSkyProfile* Profile = ResolveProfile();
	if (!Profile)
	{
		return;
	}

	const FNightfallSkyKey Key = Profile->Evaluate(SunAltitudeDegrees, bMorning);

	// --- Sun ------------------------------------------------------------------------
	//
	// The sun must stop lighting the world once it is under the horizon - a directional
	// light does not care that the atmosphere has occluded it, and leaving it on lights
	// every surface from underneath. It must NOT be hidden to achieve that. SetVisibility
	// unregisters the light, which nulls the scene's atmosphere sun; the sky atmosphere
	// then falls back to the next directional light, which here is the moon, and the bright
	// spot in the sky detaches from the light direction in a single frame. Fading the
	// illuminance across a window either side of the horizon instead keeps the light
	// registered, so the disc and its glow stay locked to the light all the way down.
	const float HorizonAlpha = FMath::SmoothStep(
		SunFadeEndAltitudeDegrees, SunFadeStartAltitudeDegrees, SunAltitudeDegrees);

	// SmoothStep returns exactly 0 below the end angle, so under it the sun contributes
	// nothing at all. The floor only keeps the light registered: a light at zero intensity
	// is dropped from the scene exactly as if it were hidden, taking the sky's sun with it.
	SunLight->SetIntensity(FMath::Max(Key.SunIntensityLux * HorizonAlpha, SunBelowHorizonFloorLux));
	// The colour goes through the default sRGB round trip. Passing bSRGB=false stores the
	// linear value unencoded, and every reader decodes the stored byte as sRGB regardless,
	// so the sun and moon were arriving a whole gamma dark and over-saturated while every
	// other light in the project - pylons, beams, the phone - was already correct.
	SunLight->SetLightColor(Key.SunColor);
	SunLight->SetLightSourceAngle(Key.SunSourceAngleDegrees);
	SunLight->SetVolumetricScatteringIntensity(Key.SunVolumetricScattering);
	SunLight->SetEnableLightShaftBloom(Key.bSunLightShaftBloom);

	// SetBloomScale marks the render state dirty, which re-registers the light. The scale
	// is lerped continuously through twilight, so setting it unconditionally rebuilt the
	// sun's scene proxy every frame for the whole crossing.
	if (!FMath::IsNearlyEqual(SunLight->BloomScale, Key.SunLightShaftBloomScale, 0.01f))
	{
		SunLight->SetBloomScale(Key.SunLightShaftBloomScale);
	}

	// --- Moon -----------------------------------------------------------------------
	MoonLight->SetIntensity(Key.MoonIntensityLux);
	MoonLight->SetLightColor(Key.MoonColor);
	MoonLight->SetVisibility(Key.MoonIntensityLux > KINDA_SMALL_NUMBER);

	// --- Sky light ------------------------------------------------------------------
	SkyLight->SetIntensity(Key.SkyLightIntensity);
	SkyLight->SetLightColor(Key.SkyLightColor);
	SkyLight->SetVolumetricScatteringIntensity(Key.SkyLightVolumetricScattering);

	// --- Fog ------------------------------------------------------------------------
	HeightFog->SetFogDensity(Key.FogDensity);
	HeightFog->SetFogHeightFalloff(Key.FogHeightFalloff);
	HeightFog->SetFogInscatteringColor(Key.FogInscatteringColor);
	HeightFog->SetVolumetricFogExtinctionScale(Key.VolumetricFogExtinctionScale);
	// Left unencoded on purpose, unlike the light colours above. This is a different round
	// trip - an FColor read back by the fog scene info rather than by a light - and it has
	// not been traced end to end. The night was calibrated against this path as it stands,
	// and changing it on the assumption that it shares the light bug washed the blue out of
	// the whole night sky.
	HeightFog->SetVolumetricFogAlbedo(Key.VolumetricFogAlbedo.ToFColor(/*bSRGB=*/false));
	HeightFog->SetVolumetricFogEmissive(Key.VolumetricFogEmissive);
	HeightFog->SetVolumetricFogScatteringDistribution(Key.VolumetricFogScatteringDistribution);

	// Fog distance is the biggest single lever on volumetric cost, so the player owns it.
	// The actor's own value is the fallback before settings exist.
	const UNightfallGameUserSettings* UserSettings = UNightfallGameUserSettings::GetNightfallSettings();
	HeightFog->SetVolumetricFogDistance(UserSettings ? UserSettings->GetVolumetricFogDistance() : VolumetricFogDistance);

	// --- Grade ----------------------------------------------------------------------
	FPostProcessSettings& Settings = PostProcess->Settings;

	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Histogram;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = Key.ExposureBias;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = AutoExposureMinBrightness;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMaxBrightness = AutoExposureMaxBrightness;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.AutoExposureSpeedUp = AutoExposureSpeedUp;
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedDown = AutoExposureSpeedDown;

	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = BloomIntensity;

	if (Profile->ColorGradingLUT)
	{
		Settings.bOverride_ColorGradingLUT = true;
		Settings.ColorGradingLUT = Profile->ColorGradingLUT;
		Settings.bOverride_ColorGradingIntensity = true;
		Settings.ColorGradingIntensity = Profile->ColorGradingIntensity;
	}
}
