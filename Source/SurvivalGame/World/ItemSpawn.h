// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "Engine/DataTable.h"
#include "ItemSpawn.generated.h"

USTRUCT(BlueprintType)
struct FLootTableRow : public FTableRowBase
{
	GENERATED_BODY()
public:
	//the items to spawn
	UPROPERTY(EditDefaultsOnly, Category="Loot")
	TArray<TSubclassOf<class UItem>> Items;

	//the percentage chance of spawning this item if we hit it on the roll
	UPROPERTY(EditDefaultsOnly, Category ="Loot", meta=(ClampMin= 0.001, ClampMax=1.0))
	float Probability = 1.f;
};

UCLASS()
class SURVIVALGAME_API AItemSpawn : public ATargetPoint
{
	GENERATED_BODY()

public:
	AItemSpawn();

	UPROPERTY(EditAnywhere, Category = "Loot")
	class UDataTable* LootTable;

	/** as pickup use blueprint base, we use uproperty to select it */
	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<class APickup> PickupClass;

	//time in second
	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	FIntPoint RespawnRange;

protected:
	FTimerHandle TimerHandle_RespawnItem;

	UPROPERTY()
	TArray<AActor*> SpawnedPickups;


	virtual void BeginPlay() override;

	UFUNCTION()
	void SpawnItem();

	// this is bound to the item being destroyed, so we cna queue up another item to be spawned in
	UFUNCTION()
	void OnItemTaken(AActor* DestroyedActor);



};
