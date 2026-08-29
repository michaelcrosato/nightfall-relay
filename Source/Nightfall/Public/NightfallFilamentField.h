// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightfallFilamentField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

/**
 * The world-position-offset element: a bed of thin filaments that sway.
 *
 * Not one vertex of this moves on the CPU. The actor scatters instances once from a seed,
 * then each frame writes three numbers - a wind vector, a gust strength and a night glow -
 * onto a single dynamic material instance. Every blade bends in the vertex shader from
 * those numbers plus its own per-instance random, which is why ten thousand of them cost
 * about the same as one.
 *
 * Scatter is deterministic: the same seed and extent always produce the same field, so
 * this is safe to rebuild on load rather than storing instance transforms in the map.
 */
UCLASS()
class NIGHTFALL_API ANightfallFilamentField : public AActor
{
	GENERATED_BODY()

public:
	ANightfallFilamentField();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Regenerate the scatter. Called automatically when the actor is edited. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nightfall|Filaments")
	void RebuildField();

	// --- Scatter ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments")
	TObjectPtr<UStaticMesh> FilamentMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments")
	int32 Seed = 20261;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments", meta = (ClampMin = "0"))
	int32 InstanceCount = 2200;

	/** Half extents of the scatter area in cm, on X and Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments")
	FVector2D Extent = FVector2D(1400.0f, 1400.0f);

	/** Height above the actor a placement trace starts from, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments", meta = (ClampMin = "0.0"))
	float TraceHeight = 900.0f;

	/** Maximum ground slope in degrees a filament will grow on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxGroundSlope = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments", meta = (ClampMin = "0.01"))
	float MinScale = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Filaments", meta = (ClampMin = "0.01"))
	float MaxScale = 1.55f;

	// --- Wind ------------------------------------------------------------------------

	/** Prevailing wind bearing in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind")
	float WindBearingDegrees = 35.0f;

	/** Baseline wind strength, roughly 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind", meta = (ClampMin = "0.0"))
	float WindStrength = 0.42f;

	/** How much a gust adds on top of the baseline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind", meta = (ClampMin = "0.0"))
	float GustAmplitude = 0.55f;

	/** Slowest of the three gust oscillators, in cycles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind", meta = (ClampMin = "0.001"))
	float GustBaseFrequency = 0.11f;

	/** Degrees the bearing wanders either side of prevailing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind")
	float BearingWanderDegrees = 22.0f;

	/** Emissive level the filaments reach at full night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind", meta = (ClampMin = "0.0"))
	float NightGlowStrength = 1.0f;

	/**
	 * Solar altitude at which the glow begins to come up. Zero on purpose: this is the sky
	 * profile's twilight key, and above it the sun still delivers thousands of lux, against
	 * which a faint emissive cannot be seen. Starting earlier only puts it out of step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind")
	float GlowStartAltitudeDegrees = 0.0f;

	/**
	 * Solar altitude at which the glow is fully up. Mirrors UNightfallSkyProfile's
	 * NightAltitudeDegrees, which is also mirrored in NightfallWorldClockSubsystem's phase
	 * classification - past that altitude everything else in the sky has stopped moving, and
	 * a glow still climbing over a frozen picture is the part anyone would notice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Wind")
	float GlowFullAltitudeDegrees = -6.0f;

	// --- Material parameter names ------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Parameters")
	FName WindVectorParameter = FName("WindVector");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Parameters")
	FName GustStrengthParameter = FName("GustStrength");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Parameters")
	FName NightGlowParameter = FName("NightGlow");

	UFUNCTION(BlueprintPure, Category = "Nightfall|Filaments")
	int32 GetPlacedCount() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Filaments;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WindMaterial;

	/** Seconds accumulated for the gust oscillators. */
	float GustTime = 0.0f;
};
