#include "InfiniteLevelGenerator.h"

#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AInfiniteLevelGenerator::AInfiniteLevelGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when this actor is spawned into the world
void AInfiniteLevelGenerator::BeginPlay()
{
	// Call the parent class's BeginPlay to ensure proper initialization
	Super::BeginPlay();

	// Get the global instance of the Customizable Object System
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	// Enable replacing discarded meshes with reference meshes to maintain visual consistency
	System->SetReplaceDiscardedWithReferenceMeshEnabled(true);

	// Retrieve the console variable for optimizing updates to customizable objects
	IConsoleVariable* OnlyUpdateCloseCustomizableObjectsCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("b.OnlyUpdateCloseCustomizableObjects"));
	
	// Ensure the console variable exists before using it
	if (ensure(OnlyUpdateCloseCustomizableObjectsCvar))
	{
		// Enable the setting to update only close customizable objects, improving performance
		OnlyUpdateCloseCustomizableObjectsCvar->Set(true);
	}	

	// Attempt to access the Level-of-Detail (LOD) management system from the Customizable Object System
	if (UCustomizableInstanceLODManagement* LODManagement = Cast<UCustomizableInstanceLODManagement>(System->GetInstanceLODManagement()))
	{
		// Set the update distance at which customizable objects will adjust their detail level
		LODManagement->SetCustomizableObjectsUpdateDistance(UpdateDistance);
	}

	// Create a default transform for spawning an actor
	FTransform Transform;

	// Begin deferred spawning of the Occluder actor to temporarily measure its size
	if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), OccluderActor, Transform))
	{
		// Complete the actor spawning immediately (without modifying its initial properties)
		UGameplayStatics::FinishSpawningActor(Actor, Transform);

		// Calculate the actor's bounding box in local space to determine its dimensions
		FBox Box = Actor->CalculateComponentsBoundingBoxInLocalSpace();

		// Store the original size of the occluder actor for later use (likely occlusion calculations)
		OriginalOccluderSize = Box.GetSize();

		// Destroy the temporary actor as it's no longer needed
		Actor->Destroy();
	}

	// Initialize the random number generator with a fixed seed (0) for repeatable procedural results
	RandomStream.Initialize(0);

	// Get the player pawn from the game world and store it in PlayerPawn
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

}


// Called when the actor is removed from the game or the level ends
void AInfiniteLevelGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Call the parent class's EndPlay to ensure proper cleanup
	Super::EndPlay(EndPlayReason);

	// Retrieve the console variable for optimizing updates to customizable objects
	IConsoleVariable* OnlyUpdateCloseCustomizableObjectsCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("b.OnlyUpdateCloseCustomizableObjects"));
	
	// Ensure the console variable exists before using it
	if (ensure(OnlyUpdateCloseCustomizableObjectsCvar))
	{
		// Disable the setting to update only close customizable objects, reverting to default behavior
		OnlyUpdateCloseCustomizableObjectsCvar->Set(false);
	}
}


// Iterates through regions in a spiral pattern around the start region
void AInfiniteLevelGenerator::IterateRegions(const FIntVector2& StartRegionCoord, const TFunction<bool(FIntVector2, int32)>& VisitRegion)
{
	// Define movement offsets for navigating regions (up, left, down, right)
	FIntVector2 Offsets[] = { FIntVector2(0, -1), FIntVector2(-1, 0), FIntVector2(0, 1), FIntVector2(1, 0) };

	// Visit the starting region first; if it returns true, stop iteration
	if (VisitRegion(StartRegionCoord, 0))
	{
		return;
	}
	
	// Determine the number of rings to iterate through, based on the widest region
	const int32 NumRing = FMath::Max(RegionWidthScenery, 0);
	for (int32 RingIndex = 1; RingIndex < NumRing; ++RingIndex)
	{
		// Calculate the number of regions in the current ring
		const int32 NumRegions = 8 * RingIndex;

		// Start at the top-right corner of the ring
		const FIntVector2 Start(RingIndex, RingIndex);
		FIntVector2 LocalRegionCoord = Start;
		
		// Iterate over all regions in the current ring
		for (int32 RegionIndex = 0; RegionIndex < NumRegions; ++RegionIndex)
		{
			// Compute the absolute region coordinate
			const FIntVector2 RegionCoord = StartRegionCoord + LocalRegionCoord;

			// Visit the region; if it returns true, stop iteration
			if (VisitRegion(RegionCoord, RingIndex))
			{
				return;
			}

			// Move to the next region based on the current ring section
			LocalRegionCoord += Offsets[RegionIndex / (RingIndex * 2)];
		}
	}
}


bool AInfiniteLevelGenerator::SpawnScenery(const FIntVector2 RegionCoord, const int32 RingIndex)
{
	if (RegionWidthScenery > RingIndex)
	{
		FRegionScenery& Region = RegionScenery.FindOrAdd(RegionCoord);

		if (Region.Scenery.IsEmpty())
		{
			const FVector RegionLocation(RegionCoord.X * RegionSize.X, RegionCoord.Y * RegionSize.Y, 0.0);

			if (PlaneActor)
			{
				FTransform Transform;
				Transform.SetLocation(RegionLocation);
				Transform.SetScale3D(FVector(RegionSize.X, RegionSize.Y, 1.0));
					
				if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), PlaneActor, Transform))
				{
					UGameplayStatics::FinishSpawningActor(Actor, Transform);

					Region.Scenery.Add(Actor);	
				}
			}

			if (OccluderActor)
			{
				for (int32 OccluderIndex = 0; OccluderIndex < OccludersNum; ++OccluderIndex)
				{
					const FVector RegionSize3D(RegionSize.X, RegionSize.Y, 1.0);
						
					FVector OccluderSize(RandomStream.FRandRange(200.0, 1000.0), RandomStream.FRandRange(200.0, 1000.0), RandomStream.RandRange(500.0, 3000.0));
						
					FVector Location(RandomStream.GetFraction(), RandomStream.GetFraction(), OccluderSize.Z * 0.5);
					Location = RegionLocation + (Location * RegionSize3D - RegionSize3D / 2.0);
						
					FTransform Transform;
					Transform.SetLocation(Location);
					Transform.SetScale3D(OccluderSize / OriginalOccluderSize);

					constexpr ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					if (AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), OccluderActor, Transform, CollisionHandling))
					{
						UGameplayStatics::FinishSpawningActor(Actor, Transform);

						Region.Scenery.Add(Actor);	
					}
				}
			}

			return true;
		}
	}
	
	return false;
}


// Called every frame
void AInfiniteLevelGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	FVector2f PlayerLocation = FVector2f::Zero();
	if (PlayerPawn)
	{
		FVector Location = PlayerPawn->GetActorLocation();
		PlayerLocation = FVector2f(Location.X, Location.Y);
		
		if (UCustomizableInstanceLODManagement* LODManagement = Cast<UCustomizableInstanceLODManagement>(System->GetInstanceLODManagement()))
		{
			LODManagement->ClearViewCenters();
			LODManagement->AddViewCenter(PlayerPawn);
		}
	}

	FVector2f CurrentRegionCoordF = (PlayerLocation / RegionSize).RoundToVector();
	FIntVector2 CurrentRegionCoord(CurrentRegionCoordF.X, CurrentRegionCoordF.Y);	

	// Destroy Regions
	for (TMap<FIntVector2, FRegionScenery>::TIterator It = RegionScenery.CreateIterator(); It; ++It)
	{
		FRegionScenery& Region = It.Value();

		const int32 DiffX = FMath::Abs(It.Key().X - CurrentRegionCoord.X);
		const int32 DiffY = FMath::Abs(It.Key().Y - CurrentRegionCoord.Y);
			
		if (DiffX > RegionWidthScenery || DiffY > RegionWidthScenery)
		{
			for (AActor* Scenery : Region.Scenery)
			{
				Scenery->Destroy();
			}
			
			It.RemoveCurrent();
		}
	}

	// Spawn Scenary
	IterateRegions(CurrentRegionCoord, [this](const FIntVector2 RegionCoord, const int32 RegionIndex)
	{
		return SpawnScenery(RegionCoord, RegionIndex);
	});

}

