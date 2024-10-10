// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/EquippableItem.h"
#include "ClothingItem.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class SURVIVALGAME_API UClothingItem : public UEquippableItem
{
	GENERATED_BODY()

public:

	UClothingItem();

	virtual bool Equip(class ASurvivalCharacter* Character) override;

	virtual bool UnEquip(class ASurvivalCharacter* Character) override;

	/** The skeletal mesh for this gear */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Clothing")
	class USkeletalMesh* Mesh;

	/** Optional material instance to apply  to the gear */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Clothing")
	class UMaterialInstance* MaterialInstance;
	
	/** Amount of defence this item provides 0.2 means 20% less damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Clothing", meta=(ClampMin=0.0, ClampMax=1.0))
	float DamageDefenceMultiplier;
};
