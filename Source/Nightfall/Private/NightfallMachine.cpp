// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallMachine.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NightfallMachineProfile.h"

ANightfallMachine::ANightfallMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	MachineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MachineRoot"));
	SetRootComponent(MachineRoot);
}

UStaticMeshComponent* ANightfallMachine::CreatePart(FName PartName, USceneComponent* AttachTo)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(PartName);
	Part->SetupAttachment(AttachTo ? AttachTo : MachineRoot.Get());
	Part->SetMobility(EComponentMobility::Movable);

	// Machines are lit expensively but queried cheaply: they cast virtual shadow maps and
	// take part in Lumen, but their collision is simple and their meshes never deform.
	Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Part->SetCollisionObjectType(ECC_WorldDynamic);
	Part->SetCastShadow(true);
	Part->bCastDynamicShadow = true;

	Parts.Add(Part);
	return Part;
}

void ANightfallMachine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyProfile();
}

void ANightfallMachine::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void ANightfallMachine::BeginPlay()
{
	Super::BeginPlay();

	ApplyProfile();
	RebuildDynamicMaterials();
	SetEmissiveLevel(EmissiveLevel);

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void ANightfallMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void ANightfallMachine::ApplyProfile()
{
	if (!Profile)
	{
		return;
	}

	for (const TObjectPtr<UStaticMeshComponent>& PartPtr : Parts)
	{
		UStaticMeshComponent* Part = PartPtr.Get();
		if (!Part)
		{
			continue;
		}

		const FNightfallMachinePart* Description = Profile->FindPart(Part->GetFName());
		if (!Description)
		{
			continue;
		}

		Part->SetStaticMesh(Description->Mesh);

		if (Description->MaterialOverride)
		{
			Part->SetMaterial(0, Description->MaterialOverride);
		}

		Part->SetRelativeTransform(FTransform(
			Description->RelativeRotation,
			Description->RelativeLocation,
			Description->RelativeScale));
	}
}

void ANightfallMachine::RebuildDynamicMaterials()
{
	EmissiveMaterials.Reset();

	if (!Profile)
	{
		return;
	}

	for (const TObjectPtr<UStaticMeshComponent>& PartPtr : Parts)
	{
		UStaticMeshComponent* Part = PartPtr.Get();
		if (!Part || !Part->GetStaticMesh())
		{
			continue;
		}

		const FNightfallMachinePart* Description = Profile->FindPart(Part->GetFName());
		if (!Description || !Description->bEmissive)
		{
			continue;
		}

		// One dynamic instance per emissive element, so a single scalar write lights the
		// whole machine.
		const int32 NumMaterials = Part->GetNumMaterials();
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			if (UMaterialInstanceDynamic* Dynamic = Part->CreateAndSetMaterialInstanceDynamic(Index))
			{
				Dynamic->SetVectorParameterValue(Profile->EmissiveColorParameter, Profile->AccentColor);
				Dynamic->SetScalarParameterValue(Profile->EmissiveIntensityParameter, Profile->AccentIntensity);
				EmissiveMaterials.Add(Dynamic);
			}
		}
	}
}

void ANightfallMachine::SetEmissiveLevel(float Level)
{
	EmissiveLevel = FMath::Clamp(Level, 0.0f, 1.0f);

	if (!Profile)
	{
		return;
	}

	for (const TObjectPtr<UMaterialInstanceDynamic>& Material : EmissiveMaterials)
	{
		if (Material)
		{
			Material->SetScalarParameterValue(Profile->EmissiveLevelParameter, EmissiveLevel);
		}
	}
}

void ANightfallMachine::SetEmissiveColor(FLinearColor Color)
{
	if (!Profile)
	{
		return;
	}

	for (const TObjectPtr<UMaterialInstanceDynamic>& Material : EmissiveMaterials)
	{
		if (Material)
		{
			Material->SetVectorParameterValue(Profile->EmissiveColorParameter, Color);
		}
	}
}

void ANightfallMachine::SetStateTag(FGameplayTag NewState)
{
	if (StateTag == NewState)
	{
		return;
	}

	StateTag = NewState;
	OnStateChanged.Broadcast(this, StateTag);
}
