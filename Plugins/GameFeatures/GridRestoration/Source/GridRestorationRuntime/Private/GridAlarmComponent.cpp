// Copyright Nightfall Relay. All Rights Reserved.

#include "GridAlarmComponent.h"

#include "GridRestorationRuntime.h"
#include "GridRestorationSubsystem.h"
#include "NightfallSentinelDrone.h"

UGridAlarmComponent::UGridAlarmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGridAlarmComponent::BeginPlay()
{
	Super::BeginPlay();

	Sentinel = Cast<ANightfallSentinelDrone>(GetOwner());
	if (!Sentinel.IsValid())
	{
		UE_LOG(LogGridRestoration, Warning,
			TEXT("UGridAlarmComponent landed on '%s', which is not a sentinel drone."), *GetNameSafe(GetOwner()));
		return;
	}

	Sentinel->OnAlertChanged.AddDynamic(this, &UGridAlarmComponent::HandleAlertChanged);

	// A drone that streams in already alerted should count immediately.
	if (Sentinel->IsAlerted())
	{
		HandleAlertChanged(Sentinel.Get(), true);
	}
}

void UGridAlarmComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANightfallSentinelDrone* Drone = Sentinel.Get())
	{
		Drone->OnAlertChanged.RemoveDynamic(this, &UGridAlarmComponent::HandleAlertChanged);
	}

	// Stop contributing to the drain even if the drone was alerted when it streamed out.
	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->SetAlarmActive(this, false);
	}

	Super::EndPlay(EndPlayReason);
}

void UGridAlarmComponent::HandleAlertChanged(ANightfallSentinelDrone* Drone, bool bAlerted)
{
	if (UGridRestorationSubsystem* Subsystem = UGridRestorationSubsystem::Get(this))
	{
		Subsystem->SetAlarmActive(this, bAlerted);
	}
}
