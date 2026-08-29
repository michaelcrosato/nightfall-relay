// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightfallMachineProfile.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * One rigid part of a machine: which component it fills, what it looks like, and where it
 * sits in its parent's space.
 *
 * PartName matches the component name declared in the actor's constructor. That is the
 * whole binding contract - a profile cannot invent parts the actor does not have, and the
 * actor never hard-references a mesh asset.
 */
USTRUCT(BlueprintType)
struct NIGHTFALL_API FNightfallMachinePart
{
	GENERATED_BODY()

	/** Component name on the target actor, for example "YawRing". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName PartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<UStaticMesh> Mesh;

	/** Optional override for element 0. Leave unset to keep the mesh's own material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FVector RelativeScale = FVector::OneVector;

	/** When true the part gets a dynamic material instance and follows the machine's emissive level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	bool bEmissive = false;
};

/**
 * The look of one machine type, as data.
 *
 * Behaviour tuning deliberately does not live here - it stays on the actor so it can be
 * varied per placement. This asset answers only "what is this made of".
 */
UCLASS(BlueprintType)
class NIGHTFALL_API UNightfallMachineProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine", meta = (TitleProperty = "PartName"))
	TArray<FNightfallMachinePart> Parts;

	/** Emissive colour for parts flagged emissive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine")
	FLinearColor AccentColor = FLinearColor(0.05f, 0.72f, 1.0f);

	/** Emissive multiplier at full level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine", meta = (ClampMin = "0.0"))
	float AccentIntensity = 22.0f;

	/** Scalar parameter driven with the emissive level, in the range [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine")
	FName EmissiveLevelParameter = FName("EmissiveLevel");

	/** Vector parameter set to AccentColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine")
	FName EmissiveColorParameter = FName("EmissiveColor");

	/** Scalar parameter set to AccentIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine")
	FName EmissiveIntensityParameter = FName("EmissiveIntensity");

	/** Find a part by component name. Returns null when the profile does not describe it. */
	const FNightfallMachinePart* FindPart(FName PartName) const;
};
