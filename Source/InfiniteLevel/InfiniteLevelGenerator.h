#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InfiniteLevelGenerator.generated.h"

USTRUCT()
struct FRegionScenery
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Scenery;
};

UCLASS()
class INFINITELEVEL_API AInfiniteLevelGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInfiniteLevelGenerator();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	float UpdateDistance = 5000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 RegionWidthScenery = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	FVector2f RegionSize = FVector2f(4000.0f, 4000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Infinite)
	int32 OccludersNum = 1;

	UPROPERTY(EditAnywhere, Category = Infinite)
	TSubclassOf<AActor> OccluderActor = nullptr;	

	UPROPERTY(EditAnywhere, Category = Infinite)
	TSubclassOf<AActor> PlaneActor = nullptr;	

	// UPROPERTY(EditAnywhere, Category = Infinite)
	TObjectPtr<APawn> PlayerPawn = nullptr;

private:
	FVector OriginalOccluderSize;

	FRandomStream RandomStream;

	UPROPERTY()
	TMap<FIntVector2, FRegionScenery> RegionScenery;

	// Own interface
	void IterateRegions(const FIntVector2& StartRegionCoord, const TFunction<bool(FIntVector2, int32)>& VisitRegion);

	bool SpawnScenery(const FIntVector2 RegionCoord, const int32 RingIndex);		


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

};
