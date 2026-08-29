// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallPhysicsProp.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NightfallCarryComponent.h"
#include "NightfallGameplayTags.h"
#include "NightfallInteractableComponent.h"

#define LOCTEXT_NAMESPACE "Nightfall"

ANightfallPhysicsProp::ANightfallPhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCastShadow(true);
	// Angular damping keeps a dropped cell from spinning forever on flat ground.
	Mesh->SetLinearDamping(0.12f);
	Mesh->SetAngularDamping(1.4f);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Lumens);
	Glow->SetAttenuationRadius(650.0f);
	Glow->SetCastShadows(true);
	Glow->SetVolumetricScatteringIntensity(2.0f);
	Glow->SetIntensity(0.0f);

	Interactable = CreateDefaultSubobject<UNightfallInteractableComponent>(TEXT("Interactable"));
	Interactable->SetupAttachment(Mesh);
	Interactable->InteractionTags.AddTag(NightfallTags::Interactable_Portable);
	Interactable->DisplayName = LOCTEXT("PropName", "Cell");
	Interactable->Verb = LOCTEXT("PropVerb", "Pick Up");
	Interactable->MaxInteractionDistanceOverride = 260.0f;
}

void ANightfallPhysicsProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAppearance();
}

void ANightfallPhysicsProp::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void ANightfallPhysicsProp::BeginPlay()
{
	Super::BeginPlay();

	ApplyAppearance();

	if (Mesh->GetStaticMesh())
	{
		DynamicMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	Interactable->OnInteracted.AddDynamic(this, &ANightfallPhysicsProp::HandleInteracted);

	SetGlowColor(GlowColor);
	SetEmissiveLevel(EmissiveLevel);

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void ANightfallPhysicsProp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void ANightfallPhysicsProp::ApplyAppearance()
{
	if (!Mesh)
	{
		return;
	}

	Mesh->SetStaticMesh(PropMesh);
	if (PropMaterial)
	{
		Mesh->SetMaterial(0, PropMaterial);
	}
	Mesh->SetMassOverrideInKg(NAME_None, MassKg, /*bNewOverrideMass=*/true);

	if (Glow)
	{
		Glow->SetVisibility(GlowIntensity > 0.0f);
	}
}

void ANightfallPhysicsProp::HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor)
{
	// Picking up is entirely the interactor's business: the prop just offers itself.
	if (UNightfallCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UNightfallCarryComponent>() : nullptr)
	{
		Carry->TryCarry(Mesh);
	}
}

void ANightfallPhysicsProp::SetEmissiveLevel(float Level)
{
	EmissiveLevel = FMath::Clamp(Level, 0.0f, 1.0f);

	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(EmissiveLevelParameter, EmissiveLevel);
	}
	if (Glow)
	{
		Glow->SetIntensity(GlowIntensity * EmissiveLevel);
	}
}

void ANightfallPhysicsProp::SetGlowColor(FLinearColor NewColor)
{
	GlowColor = NewColor;

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(EmissiveColorParameter, GlowColor);
	}
	if (Glow)
	{
		Glow->SetLightColor(GlowColor);
	}
}

void ANightfallPhysicsProp::OnNightfallPostLoad()
{
	// The transform came back from the save; wake the body so it settles against whatever
	// is under it now rather than hanging in the air.
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->WakeAllRigidBodies();
	}
	SetEmissiveLevel(EmissiveLevel);
}

#undef LOCTEXT_NAMESPACE
