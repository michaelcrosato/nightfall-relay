// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/Interface.h"
#include "NightfallSaveGame.generated.h"

/** Serialised SaveGame properties of one component. */
USTRUCT()
struct FNightfallComponentRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName ComponentName;

	UPROPERTY()
	TArray<uint8> Data;
};

/**
 * One saved actor.
 *
 * Components are recorded separately because AActor::Serialize does not reach into them,
 * and components are exactly where Game Feature Plugins put their state. A plugin marks a
 * property SaveGame and it persists, with no change to the save code.
 */
USTRUCT()
struct FNightfallActorRecord
{
	GENERATED_BODY()

	/** Placed actors keep stable names across sessions, which is what keys the record. */
	UPROPERTY()
	FName ActorName;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	TArray<uint8> Data;

	UPROPERTY()
	TArray<FNightfallComponentRecord> Components;
};

/** The contents of a save slot. */
UCLASS()
class NIGHTFALL_API UNightfallSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bumped when the layout changes; older saves are rejected rather than misread. */
	UPROPERTY()
	int32 SaveVersion = 2;

	UPROPERTY()
	FDateTime SavedAtUtc = FDateTime(0);

	UPROPERTY()
	float TimeOfDayHours = 17.5f;

	/** Days elapsed when the save was taken, so the restored world keeps its date. */
	UPROPERTY()
	int32 DayIndex = 0;

	UPROPERTY()
	FTransform PlayerTransform = FTransform::Identity;

	UPROPERTY()
	FRotator PlayerViewRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TArray<FNightfallActorRecord> ActorRecords;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UNightfallSaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Opt in to being saved.
 *
 * Implement this on an actor and every property it or its components mark SaveGame is
 * written and restored. The two hooks exist for state that cannot simply be assigned back,
 * such as snapping a mechanism to the silhouette its restored value implies.
 */
class NIGHTFALL_API INightfallSaveable
{
	GENERATED_BODY()

public:
	/** Called immediately before the actor is serialised. */
	virtual void OnNightfallPreSave() {}

	/** Called after the actor's properties have been restored. */
	virtual void OnNightfallPostLoad() {}
};
