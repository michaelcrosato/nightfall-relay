// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GridAlarmComponent.generated.h"

class ANightfallSentinelDrone;

/**
 * Connects a sentinel's alert state to the grid.
 *
 * Added to every ANightfallSentinelDrone by this feature. The drone already broadcast that
 * it had acquired a target and cared about nothing further; this component is what makes
 * being seen expensive, by telling the subsystem to start draining the grid.
 *
 * It is deliberately the smallest possible class. That is the point: the cost of hooking
 * an existing entity into a new feature should be a component and a delegate.
 */
UCLASS(ClassGroup = (GridRestoration), meta = (BlueprintSpawnableComponent))
class GRIDRESTORATIONRUNTIME_API UGridAlarmComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGridAlarmComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleAlertChanged(ANightfallSentinelDrone* Drone, bool bAlerted);

	TWeakObjectPtr<ANightfallSentinelDrone> Sentinel;
};
