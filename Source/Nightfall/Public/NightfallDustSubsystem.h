// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightfallDustSubsystem.generated.h"

/**
 * Which ground in this world is loose dust, and which is not.
 *
 * The relay field is bare basalt and it moves when anything disturbs it. The levelled
 * compound and the filament beds do not, so a drone hovering over either should raise
 * nothing. Rather than every effect working that out for itself, ground that stays clean
 * registers its own footprint here and anything that needs to know asks.
 *
 * Grass has to register rather than be traced for: ANightfallFilamentField has no collision,
 * so a downward trace passes straight through it and reports the terrain underneath.
 */
UCLASS()
class NIGHTFALL_API UNightfallDustSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	static UNightfallDustSubsystem* Get(const UObject* WorldContextObject);

	/** Claim a footprint that must read as clean. Centre and half extents in cm, XY only. */
	void RegisterCleanZone(const AActor* Owner, const FVector2D& Center, const FVector2D& HalfExtent);

	/** Give up a previously claimed footprint. Safe for an owner that never registered. */
	void UnregisterCleanZone(const AActor* Owner);

	/**
	 * 1 where the ground is bare desert, 0 well inside a clean footprint, and a smooth ramp
	 * across the edge so a plume thins out rather than switching off at a boundary.
	 */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Dust")
	float GetGroundDustiness(const FVector& WorldLocation) const;

	/** Width in cm over which dustiness feathers out at the edge of a clean zone. */
	UPROPERTY(EditAnywhere, Category = "Nightfall|Dust")
	float FeatherDistance = 900.0f;

private:
	struct FCleanZone
	{
		TWeakObjectPtr<const AActor> Owner;
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D HalfExtent = FVector2D::ZeroVector;
	};

	TArray<FCleanZone> CleanZones;
};
