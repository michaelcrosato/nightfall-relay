// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GridRestorationSubsystem.generated.h"

class ANightfallRelayPylon;
class UGridCellComponent;
class UGridNodeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGridProgressChanged);

/**
 * Scoreboard and rules for the restoration loop.
 *
 * Nodes and cells register themselves as their components come up, so the subsystem never
 * has to search the world and streaming is a non-event: a cell in an unloaded cell simply
 * is not registered yet.
 *
 * The one rule that is not a simple tally is the alarm drain. While any sentinel is
 * alerted the grid bleeds charge out of its online pylons, which is what makes being seen
 * cost something without introducing combat.
 */
UCLASS()
class GRIDRESTORATIONRUNTIME_API UGridRestorationSubsystem : public UTickableWorldSubsystem
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

	static UGridRestorationSubsystem* Get(const UObject* WorldContextObject);

	void RegisterNode(UGridNodeComponent* Node);
	void UnregisterNode(UGridNodeComponent* Node);

	void RegisterCell(UGridCellComponent* Cell);
	void UnregisterCell(UGridCellComponent* Cell);

	/** Called by UGridAlarmComponent as sentinels acquire and drop targets. */
	void SetAlarmActive(UObject* Source, bool bActive);

	// --- Progress ---------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	int32 GetNodeCount() const;

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	int32 GetNodesOnline() const;

	/** Cells that still hold charge, across every loaded cell. */
	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	int32 GetCellsRemaining() const;

	/** Total charge delivered so far, summed across every node, in node-fractions. */
	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	float GetTotalCharge() const;

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	int32 GetAlarmCount() const { return AlarmSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	bool IsAlarmActive() const { return AlarmSources.Num() > 0; }

	/** True once every registered node is online and at least one exists. */
	UFUNCTION(BlueprintPure, Category = "Grid Restoration")
	bool IsFieldRestored() const;

	/** Fires whenever a node's charge changes or a cell is spent. */
	UPROPERTY(BlueprintAssignable, Category = "Grid Restoration")
	FGridProgressChanged OnProgressChanged;

	/** Called by nodes and cells after they change something worth reporting. */
	void NotifyProgressChanged();

	/** Log the state of every registered node and cell. Exposed as Grid.Report. */
	UFUNCTION(BlueprintCallable, Category = "Grid Restoration")
	void LogReport() const;

	/**
	 * Add charge to every registered node. A cheat, for looking at a restored field
	 * without walking the route first. Exposed as Grid.Energise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Restoration")
	void EnergiseAll(float Amount);

	/** Charge drained per second from online pylons while any sentinel is alerted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Restoration")
	float AlarmDrainPerSecond = 0.035f;

private:
	/** Registered node components. Weak: pylons come and go with streaming. */
	TArray<TWeakObjectPtr<UGridNodeComponent>> Nodes;

	/** Registered cell components. */
	TArray<TWeakObjectPtr<UGridCellComponent>> Cells;

	/** Sentinels currently alerted, by source component. */
	TSet<TWeakObjectPtr<UObject>> AlarmSources;

	/** Console objects registered by this subsystem, released on shutdown. */
	TArray<IConsoleObject*> ConsoleObjects;
};
