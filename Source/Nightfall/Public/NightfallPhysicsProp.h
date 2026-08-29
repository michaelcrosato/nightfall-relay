// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NightfallSaveGame.h"
#include "NightfallPhysicsProp.generated.h"

class UMaterialInstanceDynamic;
class UNightfallInteractableComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A carryable rigid body.
 *
 * One simulating Chaos body, an interactable, and an emissive material the actor can drive.
 * It has no idea what it is for. PropTags say what a placement represents, and a Game
 * Feature Plugin attaches meaning to the tags it recognises - which is how the grid
 * restoration feature turns some of these into power cells without this class, or the
 * content that places it, knowing that feature exists.
 *
 * Registers as a modular gameplay receiver so those attachments can happen at all.
 */
UCLASS()
class NIGHTFALL_API ANightfallPhysicsProp : public AActor, public INightfallSaveable
{
	GENERATED_BODY()

public:
	ANightfallPhysicsProp();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What this placement represents. Read by feature plugins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop")
	FGameplayTagContainer PropTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop")
	TObjectPtr<UStaticMesh> PropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop")
	TObjectPtr<UMaterialInterface> PropMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop", meta = (ClampMin = "0.1"))
	float MassKg = 22.0f;

	/** Emissive tint. Also the colour of the prop's own light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Prop")
	FLinearColor GlowColor = FLinearColor(0.22f, 0.78f, 1.0f);

	/** Light output at full emissive level, in lumens. Zero omits the light entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Prop", meta = (ClampMin = "0.0"))
	float GlowIntensity = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop")
	FName EmissiveLevelParameter = FName("EmissiveLevel");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Prop")
	FName EmissiveColorParameter = FName("EmissiveColor");

	/** Drive the glow, in the range [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Prop")
	void SetEmissiveLevel(float Level);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Prop")
	float GetEmissiveLevel() const { return EmissiveLevel; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Prop")
	void SetGlowColor(FLinearColor NewColor);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Prop")
	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Prop")
	UNightfallInteractableComponent* GetInteractable() const { return Interactable; }

	//~ INightfallSaveable
	virtual void OnNightfallPostLoad() override;
	//~ End INightfallSaveable

private:
	UFUNCTION()
	void HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor);

	void ApplyAppearance();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UNightfallInteractableComponent> Interactable;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(SaveGame)
	float EmissiveLevel = 1.0f;
};
