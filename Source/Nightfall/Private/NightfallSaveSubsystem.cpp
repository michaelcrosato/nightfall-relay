// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallSaveSubsystem.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Nightfall.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallSaveGame.h"
#include "NightfallWorldClockSubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	/** Serialise only properties marked SaveGame, resolving object references by path. */
	void SerialiseSaveGameProperties(UObject& Object, TArray<uint8>& Data, bool bLoading)
	{
		if (bLoading)
		{
			FMemoryReader Reader(Data, /*bIsPersistent=*/true);
			FObjectAndNameAsStringProxyArchive Archive(Reader, /*bInLoadIfFindFails=*/true);
			Archive.ArIsSaveGame = true;
			Object.Serialize(Archive);
		}
		else
		{
			FMemoryWriter Writer(Data, /*bIsPersistent=*/true);
			FObjectAndNameAsStringProxyArchive Archive(Writer, /*bInLoadIfFindFails=*/true);
			Archive.ArIsSaveGame = true;
			Object.Serialize(Archive);
		}
	}
}

void UNightfallSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.Save"),
		TEXT("Write the current world to the Nightfall save slot."),
		FConsoleCommandDelegate::CreateLambda([this]() { SaveGame(); }),
		ECVF_Default));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.Load"),
		TEXT("Restore the Nightfall save slot into the current world."),
		FConsoleCommandDelegate::CreateLambda([this]() { LoadGame(); }),
		ECVF_Default));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.DeleteSave"),
		TEXT("Delete the Nightfall save slot."),
		FConsoleCommandDelegate::CreateLambda([this]() { DeleteSave(); }),
		ECVF_Default));
}

void UNightfallSaveSubsystem::Deinitialize()
{
	for (IConsoleObject* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();

	Super::Deinitialize();
}

UNightfallSaveSubsystem* UNightfallSaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNightfallSaveSubsystem>() : nullptr;
}

FString UNightfallSaveSubsystem::GetSlotName() const
{
	return UNightfallRuntimeSettings::Get().SaveSlotName;
}

bool UNightfallSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(GetSlotName(), 0);
}

bool UNightfallSaveSubsystem::DeleteSave()
{
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(GetSlotName(), 0);
	UE_LOG(LogNightfall, Log, TEXT("Save slot '%s' delete %s."), *GetSlotName(), bDeleted ? TEXT("succeeded") : TEXT("failed"));
	return bDeleted;
}

bool UNightfallSaveSubsystem::SaveGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OnSaveCompleted.Broadcast(false);
		return false;
	}

	UNightfallSaveGame* Save = Cast<UNightfallSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UNightfallSaveGame::StaticClass()));
	if (!Save)
	{
		OnSaveCompleted.Broadcast(false);
		return false;
	}

	Save->SavedAtUtc = FDateTime::UtcNow();

	if (const UNightfallWorldClockSubsystem* Clock = World->GetSubsystem<UNightfallWorldClockSubsystem>())
	{
		Save->TimeOfDayHours = Clock->GetTimeOfDayHours();
		Save->DayIndex = Clock->GetDayIndex();
	}

	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Save->PlayerTransform = Pawn->GetActorTransform();
		if (const AController* Controller = Pawn->GetController())
		{
			Save->PlayerViewRotation = Controller->GetControlRotation();
		}
	}

	CaptureActors(*Save);

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, GetSlotName(), 0);
	UE_LOG(LogNightfall, Log, TEXT("Saved %d actors to slot '%s': %s."),
		Save->ActorRecords.Num(), *GetSlotName(), bSaved ? TEXT("ok") : TEXT("failed"));

	OnSaveCompleted.Broadcast(bSaved);
	return bSaved;
}

void UNightfallSaveSubsystem::CaptureActors(UNightfallSaveGame& Save) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UNightfallSaveable>())
		{
			continue;
		}

		if (INightfallSaveable* Saveable = Cast<INightfallSaveable>(Actor))
		{
			Saveable->OnNightfallPreSave();
		}

		FNightfallActorRecord Record;
		Record.ActorName = Actor->GetFName();
		Record.Transform = Actor->GetActorTransform();
		SerialiseSaveGameProperties(*Actor, Record.Data, /*bLoading=*/false);

		// Components carry most of the interesting state, including anything a Game
		// Feature Plugin has attached.
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component)
			{
				continue;
			}

			FNightfallComponentRecord ComponentRecord;
			ComponentRecord.ComponentName = Component->GetFName();
			SerialiseSaveGameProperties(*Component, ComponentRecord.Data, /*bLoading=*/false);

			// An empty payload means the component had no SaveGame properties at all.
			if (ComponentRecord.Data.Num() > 0)
			{
				Record.Components.Add(MoveTemp(ComponentRecord));
			}
		}

		Save.ActorRecords.Add(MoveTemp(Record));
	}
}

bool UNightfallSaveSubsystem::LoadGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OnLoadCompleted.Broadcast(false);
		return false;
	}

	UNightfallSaveGame* Save = Cast<UNightfallSaveGame>(
		UGameplayStatics::LoadGameFromSlot(GetSlotName(), 0));
	if (!Save)
	{
		UE_LOG(LogNightfall, Warning, TEXT("No save in slot '%s'."), *GetSlotName());
		OnLoadCompleted.Broadcast(false);
		return false;
	}

	if (Save->SaveVersion != UNightfallSaveGame::StaticClass()->GetDefaultObject<UNightfallSaveGame>()->SaveVersion)
	{
		UE_LOG(LogNightfall, Warning,
			TEXT("Save in slot '%s' is version %d; this build expects %d. Refusing to load it."),
			*GetSlotName(), Save->SaveVersion,
			UNightfallSaveGame::StaticClass()->GetDefaultObject<UNightfallSaveGame>()->SaveVersion);
		OnLoadCompleted.Broadcast(false);
		return false;
	}

	if (UNightfallWorldClockSubsystem* Clock = World->GetSubsystem<UNightfallWorldClockSubsystem>())
	{
		Clock->SetDayIndex(Save->DayIndex);
		Clock->SetTimeOfDayHours(Save->TimeOfDayHours);
	}

	RestoreActors(*Save);

	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Pawn->SetActorTransform(Save->PlayerTransform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(Save->PlayerViewRotation);
		}
	}

	UE_LOG(LogNightfall, Log, TEXT("Loaded %d actor records from slot '%s'."),
		Save->ActorRecords.Num(), *GetSlotName());

	OnLoadCompleted.Broadcast(true);
	return true;
}

void UNightfallSaveSubsystem::RestoreActors(const UNightfallSaveGame& Save) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Index the world once rather than scanning it per record.
	TMap<FName, AActor*> SaveableActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->Implements<UNightfallSaveable>())
		{
			SaveableActors.Add(Actor->GetFName(), Actor);
		}
	}

	for (const FNightfallActorRecord& Record : Save.ActorRecords)
	{
		AActor** Found = SaveableActors.Find(Record.ActorName);
		if (!Found || !*Found)
		{
			// The cell holding this actor is not streamed in. Skipping is correct: the
			// record stays in the save and applies the next time it is loaded with the
			// actor present.
			continue;
		}

		AActor* Actor = *Found;
		Actor->SetActorTransform(Record.Transform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

		TArray<uint8> ActorData = Record.Data;
		SerialiseSaveGameProperties(*Actor, ActorData, /*bLoading=*/true);

		for (const FNightfallComponentRecord& ComponentRecord : Record.Components)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component && Component->GetFName() == ComponentRecord.ComponentName)
				{
					TArray<uint8> ComponentData = ComponentRecord.Data;
					SerialiseSaveGameProperties(*Component, ComponentData, /*bLoading=*/true);
					break;
				}
			}
		}

		if (INightfallSaveable* Saveable = Cast<INightfallSaveable>(Actor))
		{
			Saveable->OnNightfallPostLoad();
		}
	}
}
