// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GridCellComponent.generated.h"

class ANightfallPhysicsProp;

/**
 * Turns a carryable prop into a power cell.
 *
 * Added to every ANightfallPhysicsProp by this feature. Placements that do not carry the
 * Nightfall.Grid.PowerCell tag deactivate themselves immediately, so the same action can
 * blanket every prop in the world and only the intended ones become cells. That tag filter
 * is what lets the level place rubble and cells with one actor class.
 */
UCLASS(ClassGroup = (GridRestoration), meta = (BlueprintSpawnableComponent))
class GRIDRESTORATIONRUNTIME_API UGridCellComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGridCellComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Fraction of a pylon this cell fills. A half means two cells per pylon: enough that
	 * the field reads as a route to plan rather than a checklist to tick, without turning
	 * the slice into a haulage job.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Grid Restoration", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ChargeUnits = 0.5f;

	/** True once delivered. Persisted, so a spent cell stays spent across a save. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Grid Restoration")
	bool bSpent = false;

	/** True when this placement is a live cell that can still be delivered. */
	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	bool IsLive() const { return bIsCell && !bSpent; }

	/** Spend the cell. Dims the prop and leaves it in the world as an inert husk. */
	UFUNCTION(BlueprintCallable, Category = "Grid Restoration")
	void Consume();

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	ANightfallPhysicsProp* GetProp() const { return Prop.Get(); }

private:
	/** Push the cell's look and prompt onto the prop. */
	void ApplyPresentation();

	TWeakObjectPtr<ANightfallPhysicsProp> Prop;

	/** False when this prop placement was never meant to be a cell. */
	bool bIsCell = false;
};
