// Copyright Nightfall Relay. All Rights Reserved.

#include "GridRestorationSubsystem.h"

#include "Engine/World.h"
#include "GridCellComponent.h"
#include "HAL/IConsoleManager.h"
#include "NightfallRelayPylon.h"
#include "GridNodeComponent.h"
#include "GridRestorationRuntime.h"

void UGridRestorationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Grid.Report"),
		TEXT("Log the state of every relay node and power cell."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { LogReport(); }),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Grid.Energise"),
		TEXT("Grid.Energise <amount> - add charge to every node. Cheat."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			EnergiseAll(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.0f);
		}),
		ECVF_Cheat));
}

void UGridRestorationSubsystem::Deinitialize()
{
	for (IConsoleObject* Object : ConsoleObjects)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleObjects.Empty();

	Super::Deinitialize();
}

void UGridRestorationSubsystem::LogReport() const
{
	UE_LOG(LogGridRestoration, Log, TEXT("GRID %d/%d nodes online, %d cells live, %d sentinels tracking"),
		GetNodesOnline(), GetNodeCount(), GetCellsRemaining(), AlarmSources.Num());

	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		if (const UGridNodeComponent* Node = Weak.Get())
		{
			UE_LOG(LogGridRestoration, Log, TEXT("GRID   %-24s charge %.2f%s"),
				*GetNameSafe(Node->GetPylon()), Node->GetChargeLevel(),
				Node->IsOnline() ? TEXT(" (online)") : TEXT(""));
		}
	}
}

void UGridRestorationSubsystem::EnergiseAll(float Amount)
{
	int32 Touched = 0;
	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		if (UGridNodeComponent* Node = Weak.Get())
		{
			if (ANightfallRelayPylon* Pylon = Node->GetPylon())
			{
				Pylon->AddCharge(Amount);
				++Touched;
			}
		}
	}

	UE_LOG(LogGridRestoration, Log, TEXT("GRID energised %d nodes by %.2f."), Touched, Amount);
	NotifyProgressChanged();
}

bool UGridRestorationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGridRestorationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGridRestorationSubsystem, STATGROUP_Tickables);
}

UGridRestorationSubsystem* UGridRestorationSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGridRestorationSubsystem>() : nullptr;
}

void UGridRestorationSubsystem::RegisterNode(UGridNodeComponent* Node)
{
	if (Node)
	{
		Nodes.AddUnique(Node);
		NotifyProgressChanged();
	}
}

void UGridRestorationSubsystem::UnregisterNode(UGridNodeComponent* Node)
{
	if (Node)
	{
		Nodes.RemoveSingleSwap(Node, EAllowShrinking::No);
		NotifyProgressChanged();
	}
}

void UGridRestorationSubsystem::RegisterCell(UGridCellComponent* Cell)
{
	if (Cell)
	{
		Cells.AddUnique(Cell);
		NotifyProgressChanged();
	}
}

void UGridRestorationSubsystem::UnregisterCell(UGridCellComponent* Cell)
{
	if (Cell)
	{
		Cells.RemoveSingleSwap(Cell, EAllowShrinking::No);
		NotifyProgressChanged();
	}
}

void UGridRestorationSubsystem::SetAlarmActive(UObject* Source, bool bActive)
{
	if (!Source)
	{
		return;
	}

	const int32 Before = AlarmSources.Num();

	if (bActive)
	{
		AlarmSources.Add(Source);
	}
	else
	{
		AlarmSources.Remove(Source);
	}

	if (AlarmSources.Num() != Before)
	{
		NotifyProgressChanged();
	}
}

void UGridRestorationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_GridRestoration);

	// Drop any alarm sources whose drone has streamed out while still alerted, otherwise
	// the grid would drain forever with nothing on screen to explain it.
	AlarmSources.Remove(TWeakObjectPtr<UObject>());
	for (auto It = AlarmSources.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (AlarmSources.Num() == 0)
	{
		return;
	}

	// Drain scales with how many sentinels have eyes on the player.
	const float Drain = AlarmDrainPerSecond * AlarmSources.Num() * DeltaTime;

	bool bChanged = false;
	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		UGridNodeComponent* Node = Weak.Get();
		if (Node && Node->GetChargeLevel() > 0.0f)
		{
			Node->DrainCharge(Drain);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		NotifyProgressChanged();
	}
}

int32 UGridRestorationSubsystem::GetNodeCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		if (Weak.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

int32 UGridRestorationSubsystem::GetNodesOnline() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		if (const UGridNodeComponent* Node = Weak.Get())
		{
			Count += Node->IsOnline() ? 1 : 0;
		}
	}
	return Count;
}

int32 UGridRestorationSubsystem::GetCellsRemaining() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UGridCellComponent>& Weak : Cells)
	{
		if (const UGridCellComponent* Cell = Weak.Get())
		{
			Count += Cell->IsLive() ? 1 : 0;
		}
	}
	return Count;
}

float UGridRestorationSubsystem::GetTotalCharge() const
{
	float Total = 0.0f;
	for (const TWeakObjectPtr<UGridNodeComponent>& Weak : Nodes)
	{
		if (const UGridNodeComponent* Node = Weak.Get())
		{
			Total += Node->GetChargeLevel();
		}
	}
	return Total;
}

bool UGridRestorationSubsystem::IsFieldRestored() const
{
	const int32 Count = GetNodeCount();
	return Count > 0 && GetNodesOnline() == Count;
}

void UGridRestorationSubsystem::NotifyProgressChanged()
{
	OnProgressChanged.Broadcast();
}
