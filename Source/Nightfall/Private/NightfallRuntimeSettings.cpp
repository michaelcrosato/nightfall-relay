// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallRuntimeSettings.h"

UNightfallRuntimeSettings::UNightfallRuntimeSettings()
	: DayLengthMinutes(36.0f)
	, StartTimeOfDayHours(17.85f)
	, SunriseAzimuthDegrees(-75.0f)
	, MaxSunAltitudeDegrees(52.0f)
	, StartDate(TEXT("2231-11-04"))
	, NightTemperatureCelsius(-6.0f)
	, DayTemperatureCelsius(14.0f)
	, ThermalLagHours(3.0f)
	, TemperatureUnit(ENightfallTemperatureUnit::Celsius)
	, InteractionTraceDistance(320.0f)
	, InteractionTraceRadius(12.0f)
	, TargetFrameRate(60.0f)
	, HitchThresholdMilliseconds(12.0f)
	, SaveSlotName(TEXT("NightfallRelay"))
	, bShowBriefingOnStart(true)
	, CompoundCleanHalfExtent(6400.0f, 6400.0f)
{
}

const UNightfallRuntimeSettings& UNightfallRuntimeSettings::Get()
{
	const UNightfallRuntimeSettings* Settings = GetDefault<UNightfallRuntimeSettings>();
	check(Settings);
	return *Settings;
}
