// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NightfallInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** One tag-to-action binding. */
USTRUCT(BlueprintType)
struct NIGHTFALL_API FNightfallInputActionBinding
{
	GENERATED_BODY()

	/** A tag under Nightfall.Input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Nightfall.Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;
};

/**
 * Maps gameplay tags to Enhanced Input actions.
 *
 * Code binds by tag, never by asset pointer. That indirection is what lets a Game Feature
 * Plugin ship its own config with its own actions and bind them the same way the core
 * character does, without either side knowing about the other.
 */
UCLASS(BlueprintType, Const)
class NIGHTFALL_API UNightfallInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Context pushed when the owning pawn is possessed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Higher wins where two contexts map the same key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FNightfallInputActionBinding> Actions;

	/** The action for a tag, or null. Logs once per missing tag rather than failing silently. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	const UInputAction* FindAction(FGameplayTag InputTag, bool bLogIfMissing = true) const;
};
