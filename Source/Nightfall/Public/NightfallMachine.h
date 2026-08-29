// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NightfallSaveGame.h"
#include "NightfallMachine.generated.h"

class UMaterialInstanceDynamic;
class UNightfallMachineProfile;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNightfallMachineStateSignature, class ANightfallMachine*, Machine, FGameplayTag, NewState);

/**
 * Base for every moving thing in the world.
 *
 * A machine is a tree of static mesh components whose relative transforms are driven in
 * code. There is no skeleton, no animation asset and no pose - a turret is a base, a yaw
 * ring, a pitch arm and a barrel, and it moves because something writes rotations onto
 * them. Every entity in this project is built that way.
 *
 * The class supplies the three things all of them need: appearance from a profile asset,
 * a state tag other systems can observe, and a single emissive level that drives every
 * glowing surface at once.
 *
 * Machines are modular gameplay receivers, so a Game Feature Plugin can attach components
 * to them by class without any machine knowing the plugin exists.
 */
UCLASS(Abstract)
class NIGHTFALL_API ANightfallMachine : public AActor, public INightfallSaveable
{
	GENERATED_BODY()

public:
	ANightfallMachine();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Appearance. Assigned per placement by the content builder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Machine")
	TObjectPtr<UNightfallMachineProfile> Profile;

	UFUNCTION(BlueprintPure, Category = "Nightfall|Machine")
	FGameplayTag GetStateTag() const { return StateTag; }

	/** Change state and notify listeners. Ignores a set to the state already held. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Machine")
	void SetStateTag(FGameplayTag NewState);

	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Machine")
	FNightfallMachineStateSignature OnStateChanged;

	/** Drive every emissive surface on this machine at once, in the range [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Machine")
	void SetEmissiveLevel(float Level);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Machine")
	float GetEmissiveLevel() const { return EmissiveLevel; }

	/** Override the accent colour at runtime, for example to flash an alert. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Machine")
	void SetEmissiveColor(FLinearColor Color);

protected:
	/**
	 * Create a mesh part and attach it. Subclasses call this from their constructor; the
	 * name given here is what a profile's PartName must match.
	 */
	UStaticMeshComponent* CreatePart(FName PartName, USceneComponent* AttachTo);

	/** Push the profile onto the component tree. Safe to call in editor and at runtime. */
	void ApplyProfile();

	/** Root every part hangs from. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> MachineRoot;

	/** Every part created through CreatePart, in creation order. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

private:
	void RebuildDynamicMaterials();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EmissiveMaterials;

	UPROPERTY(VisibleInstanceOnly, Category = "Nightfall|Machine")
	FGameplayTag StateTag;

	float EmissiveLevel = 0.0f;
};
