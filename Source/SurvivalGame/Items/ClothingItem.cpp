// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/ClothingItem.h"
#include "Player/SurvivalCharacter.h"

UClothingItem::UClothingItem()
{
	DamageDefenceMultiplier = 0.1f;
}

bool UClothingItem::Equip(class ASurvivalCharacter* Character)
{
	bool bEquipSuccessful = Super::Equip(Character);

	if (bEquipSuccessful && Character) {
		Character->EquipClothing(this);
	}

	return bEquipSuccessful;
}

bool UClothingItem::UnEquip(class ASurvivalCharacter* Character)
{
	bool bUnEquipSuccessful = Super::UnEquip(Character);

	if (bUnEquipSuccessful && Character) {
		Character->UnEquipClothing(Slot);
	}

	return bUnEquipSuccessful;
}
