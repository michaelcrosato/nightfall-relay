// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightfallSentinelDrone.h"
#include "NightfallContentTools.generated.h"

class UDataTable;
class UGameFeatureData;
class UPCGGraph;
class UStaticMesh;

/**
 * Editor-only helpers the content build calls from Python.
 *
 * Two things the scripting layer cannot express on its own end up here. Game feature
 * component entries are a plain USTRUCT with no BlueprintType, so Python cannot construct
 * one; and adding a row to a DataTable is an editor-only operation the blueprint library
 * does not expose. Both are small, mechanical, and belong in the editor module rather than
 * in the runtime.
 */
UCLASS()
class NIGHTFALLEDITOR_API UNightfallContentTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replace a feature's action list with a single AddComponents action.
	 *
	 * Classes are named by path, for example "/Script/Nightfall.NightfallRelayPylon", so the
	 * caller never has to hold a UClass. The two arrays are parallel: entry i attaches
	 * ComponentClassPaths[i] to every actor of ActorClassPaths[i].
	 *
	 * @return true when every pair resolved and the action was assigned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Content")
	static bool ConfigureAddComponentsAction(
		UGameFeatureData* FeatureData,
		const TArray<FString>& ActorClassPaths,
		const TArray<FString>& ComponentClassPaths);

	/** Add or replace one sentinel tuning row. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Content")
	static bool AddSentinelTuningRow(UDataTable* Table, FName RowName, FNightfallSentinelTuningRow Row);

	/**
	 * Rebuild a PCG graph as a debris scatter: a seeded grid over the volume's floor,
	 * jittered so it does not read as a lattice, with one of the given meshes at each point.
	 *
	 * Built here rather than from Python because the mesh selector is an instanced object
	 * whose entries are a plain USTRUCT, and pin labels are read off the nodes rather than
	 * assumed. The scatter is deterministic for a given seed. Any existing nodes are removed
	 * first, so rebuilding onto the same asset replaces the graph instead of appending to it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Content")
	static bool BuildDebrisScatterGraph(
		UPCGGraph* Graph,
		const TArray<UStaticMesh*>& Meshes,
		float PointsPerSquaredMeter,
		float GridHalfExtent,
		int32 Seed);
};
