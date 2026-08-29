// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GridHudComponent.generated.h"

class SGridObjectivePanel;

/**
 * Puts this feature's objective panel on the HUD.
 *
 * Added to the player character by this feature, so the panel's lifetime is exactly the
 * feature's lifetime: activate the feature and the panel appears, deactivate it and the
 * panel goes away. The core HUD never learns that this feature exists - it only ever sees
 * a widget arriving in a named layer.
 */
UCLASS(ClassGroup = (GridRestoration), meta = (BlueprintSpawnableComponent))
class GRIDRESTORATIONRUNTIME_API UGridHudComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGridHudComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SGridObjectivePanel> Panel;
};
