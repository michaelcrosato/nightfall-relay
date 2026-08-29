// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallContentTools.h"

#include "Engine/DataTable.h"
#include "GameFeatureAction_AddComponents.h"
#include "GameFeatureData.h"
#include "NightfallEditor.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGTransformPoints.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	/** Label of a node's first output pin, or None. */
	FName FirstOutputLabel(const UPCGNode* Node)
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = Node->GetOutputPins();
		return Pins.Num() > 0 ? Pins[0]->Properties.Label : NAME_None;
	}

	/** Label of a node's first input pin, or None. */
	FName FirstInputLabel(const UPCGNode* Node)
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = Node->GetInputPins();
		return Pins.Num() > 0 ? Pins[0]->Properties.Label : NAME_None;
	}

	/**
	 * Strip every node from a graph, leaving only its input and output.
	 *
	 * The graph asset is reused across content rebuilds, and AddNodeOfType appends. Without
	 * this, each rebuild left the previous run's nodes in place: the shipped graph had seven
	 * copies of the same sampler and spawner, all doing the same work.
	 */
	void ClearGraphNodes(UPCGGraph* Graph)
	{
		TArray<UPCGNode*> Existing = Graph->GetNodes();
		for (UPCGNode* Node : Existing)
		{
			if (Node && Node != Graph->GetInputNode() && Node != Graph->GetOutputNode())
			{
				Graph->RemoveNode(Node);
			}
		}
	}
}

bool UNightfallContentTools::ConfigureAddComponentsAction(
	UGameFeatureData* FeatureData,
	const TArray<FString>& ActorClassPaths,
	const TArray<FString>& ComponentClassPaths)
{
	if (!FeatureData)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("ConfigureAddComponentsAction: no feature data."));
		return false;
	}

	if (ActorClassPaths.Num() != ComponentClassPaths.Num())
	{
		UE_LOG(LogNightfallEditor, Error,
			TEXT("ConfigureAddComponentsAction: %d actor classes but %d component classes."),
			ActorClassPaths.Num(), ComponentClassPaths.Num());
		return false;
	}

	UGameFeatureAction_AddComponents* Action =
		NewObject<UGameFeatureAction_AddComponents>(FeatureData, NAME_None, RF_Transactional);

	bool bAllResolved = true;
	for (int32 Index = 0; Index < ActorClassPaths.Num(); ++Index)
	{
		const FSoftObjectPath ActorPath(ActorClassPaths[Index]);
		const FSoftObjectPath ComponentPath(ComponentClassPaths[Index]);

		// Resolve now so a typo in the build script fails here rather than silently
		// producing a feature that adds nothing.
		if (!ActorPath.ResolveObject() && !ActorPath.TryLoad())
		{
			UE_LOG(LogNightfallEditor, Error, TEXT("Unresolved actor class '%s'."), *ActorClassPaths[Index]);
			bAllResolved = false;
			continue;
		}
		if (!ComponentPath.ResolveObject() && !ComponentPath.TryLoad())
		{
			UE_LOG(LogNightfallEditor, Error, TEXT("Unresolved component class '%s'."), *ComponentClassPaths[Index]);
			bAllResolved = false;
			continue;
		}

		FGameFeatureComponentEntry Entry;
		Entry.ActorClass = TSoftClassPtr<AActor>(ActorPath);
		Entry.ComponentClass = TSoftClassPtr<UActorComponent>(ComponentPath);
		Entry.bClientComponent = true;
		Entry.bServerComponent = true;

		Action->ComponentList.Add(MoveTemp(Entry));
	}

	if (!bAllResolved)
	{
		return false;
	}

	// Actions is protected; the class exposes an editor-only mutable accessor precisely
	// for tooling like this.
	TArray<TObjectPtr<UGameFeatureAction>>& Actions = FeatureData->GetMutableActionsInEditor();
	Actions.Reset();
	Actions.Add(Action);
	FeatureData->MarkPackageDirty();

	UE_LOG(LogNightfallEditor, Log, TEXT("%s: %d component bindings."),
		*FeatureData->GetName(), Action->ComponentList.Num());
	return true;
}

bool UNightfallContentTools::AddSentinelTuningRow(UDataTable* Table, FName RowName, FNightfallSentinelTuningRow Row)
{
	if (!Table)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("AddSentinelTuningRow: no table."));
		return false;
	}

	if (Table->GetRowStruct() != FNightfallSentinelTuningRow::StaticStruct())
	{
		UE_LOG(LogNightfallEditor, Error,
			TEXT("AddSentinelTuningRow: '%s' holds rows of a different type."), *Table->GetName());
		return false;
	}

	Table->AddRow(RowName, Row);
	Table->MarkPackageDirty();
	return true;
}

bool UNightfallContentTools::BuildDebrisScatterGraph(
	UPCGGraph* Graph,
	const TArray<UStaticMesh*>& Meshes,
	float PointsPerSquaredMeter,
	float GridHalfExtent,
	int32 Seed)
{
	if (!Graph)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: no graph."));
		return false;
	}
	if (Meshes.Num() == 0)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: no meshes to scatter."));
		return false;
	}

	ClearGraphNodes(Graph);

	// --- lay a deterministic grid over the volume ------------------------------------
	// A surface sampler was the obvious node here and it is the wrong one: it needs a
	// surface, and this world has no landscape - the ground is static meshes. Fed the
	// volume's own data it logged "No surfaces found from which to generate" on every
	// launch and scattered nothing. A grid over the volume's bounds is what the design
	// actually wanted anyway: points on the compound's levelled floor.
	UPCGCreatePointsGridSettings* GridSettings = nullptr;
	UPCGNode* GridNode = Graph->AddNodeOfType(GridSettings);
	if (!GridNode || !GridSettings)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: could not add the grid."));
		return false;
	}

	// One point per 1/PointsPerSquaredMeter of floor, expressed as a cell edge in cm.
	const float CellEdgeCentimetres = FMath::Sqrt(1.0f / FMath::Max(PointsPerSquaredMeter, KINDA_SMALL_NUMBER)) * 100.0f;
	GridSettings->CellSize = FVector(CellEdgeCentimetres, CellEdgeCentimetres, CellEdgeCentimetres);
	// Half the volume in X and Y, and flat in Z so the grid is a single layer on the floor
	// rather than a solid block of debris. Culling then trims it to the volume exactly.
	GridSettings->GridExtents = FVector(GridHalfExtent, GridHalfExtent, 0.0);
	GridSettings->CoordinateSpace = EPCGCoordinateSpace::LocalComponent;
	GridSettings->bCullPointsOutsideVolume = true;
	GridSettings->bSetPointsBounds = true;
	GridSettings->PointSteepness = 0.6f;
	GridSettings->Seed = Seed;

	// --- jitter it so it does not read as a lattice -----------------------------------
	UPCGTransformPointsSettings* JitterSettings = nullptr;
	UPCGNode* JitterNode = Graph->AddNodeOfType(JitterSettings);
	if (!JitterNode || !JitterSettings)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: could not add the jitter."));
		return false;
	}

	// Offsets stay inside the cell so the scatter keeps its even coverage, and Z is left
	// alone because the debris belongs on the floor.
	const double Jitter = CellEdgeCentimetres * 0.42;
	JitterSettings->OffsetMin = FVector(-Jitter, -Jitter, 0.0);
	JitterSettings->OffsetMax = FVector(Jitter, Jitter, 0.0);
	JitterSettings->bAbsoluteOffset = false;
	JitterSettings->RotationMin = FRotator(0.0, 0.0, 0.0);
	JitterSettings->RotationMax = FRotator(0.0, 360.0, 0.0);
	JitterSettings->bAbsoluteRotation = false;
	JitterSettings->ScaleMin = FVector(0.55);
	JitterSettings->ScaleMax = FVector(1.65);
	JitterSettings->bAbsoluteScale = false;
	JitterSettings->bUniformScale = true;
	JitterSettings->Seed = Seed;

	// --- spawn a mesh at each point ---------------------------------------------------
	UPCGStaticMeshSpawnerSettings* SpawnerSettings = nullptr;
	UPCGNode* SpawnerNode = Graph->AddNodeOfType(SpawnerSettings);
	if (!SpawnerNode || !SpawnerSettings)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: could not add the spawner."));
		return false;
	}

	UPCGMeshSelectorWeighted* Selector =
		NewObject<UPCGMeshSelectorWeighted>(SpawnerSettings, NAME_None, RF_Transactional);
	for (UStaticMesh* Mesh : Meshes)
	{
		if (Mesh)
		{
			Selector->MeshEntries.Emplace(TSoftObjectPtr<UStaticMesh>(Mesh), 1);
		}
	}

	SpawnerSettings->MeshSelectorType = UPCGMeshSelectorWeighted::StaticClass();
	SpawnerSettings->MeshSelectorParameters = Selector;
	SpawnerSettings->Seed = Seed;

	// --- wire it up --------------------------------------------------------------------
	// Pin labels are read off the nodes rather than hard coded, so this keeps working if
	// the engine renames a default pin.
	UPCGNode* InputNode = Graph->GetInputNode();
	UPCGNode* OutputNode = Graph->GetOutputNode();
	if (!InputNode || !OutputNode)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: graph has no input or output node."));
		return false;
	}

	const bool bWired =
		Graph->AddEdge(InputNode, FirstOutputLabel(InputNode), GridNode, FirstInputLabel(GridNode)) &&
		Graph->AddEdge(GridNode, FirstOutputLabel(GridNode), JitterNode, FirstInputLabel(JitterNode)) &&
		Graph->AddEdge(JitterNode, FirstOutputLabel(JitterNode), SpawnerNode, FirstInputLabel(SpawnerNode)) &&
		Graph->AddEdge(SpawnerNode, FirstOutputLabel(SpawnerNode), OutputNode, FirstInputLabel(OutputNode));

	if (!bWired)
	{
		UE_LOG(LogNightfallEditor, Error, TEXT("BuildDebrisScatterGraph: could not connect the nodes."));
		return false;
	}

	Graph->MarkPackageDirty();
	UE_LOG(LogNightfallEditor, Log, TEXT("Debris scatter graph: %d meshes on a %.0f cm grid, seed %d."),
		Selector->MeshEntries.Num(), CellEdgeCentimetres, Seed);
	return true;
}
