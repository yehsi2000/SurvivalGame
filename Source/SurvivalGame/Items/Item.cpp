// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Item.h"
#include "Components/InventoryComponent.h"
#include "Net/UnrealNetwork.h"

//Localization system - LocText
// FText is intended to be shown to user in-game
// otherwise we use FString

#define LOCTEXT_NAMESPACE "Item"

void UItem::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Server will be managing these values, synced automatically
	DOREPLIFETIME(UItem, Quantity);
}

bool UItem::IsSupportedForNetworking() const
{
	return true;
}

class UWorld* UItem::GetWorld() const
{
	return World;
}

#if WITH_EDITOR
void UItem::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName ChangedPropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// UPROPERTY clamping doesn't support using variable to clamp so we do that here
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UItem, Quantity)) {
		Quantity = FMath::Clamp(Quantity, 1, bStackable ? MaxStackSize : 1);
	}
}
#endif

UItem::UItem()
{
	// takes (key : text) set and set ftext, localization system changes text value of translated item with key
	ItemDisplayName = LOCTEXT("ItemName", "Item");
	UseActionText = LOCTEXT("ItemUseActionText", "Use");
	Weight = 0.f;
	bStackable = true;
	Quantity =  1;
	MaxStackSize = 2;
	RepKey = 0;
}

void UItem::OnRep_Quantity()
{
	OnItemModified.Broadcast();
}

void UItem::SetQuantity(const int32 NewQuantity)
{
	if (NewQuantity != Quantity)
	{
		Quantity = FMath::Clamp(NewQuantity,0,MaxStackSize);
		MarkDirtyForReplication();
	}
		
}

bool UItem::ShouldShowInInventory() const
{
	return true;
}

void UItem::Use(class ASurvivalCharacter* Character)
{

}

void UItem::AddedToInventory(class UInventoryComponent* Inventory)
{

}

void UItem::MarkDirtyForReplication()
{
	//Mark this object for replication
	++RepKey;

	//Mark the array for replication
	if (OwningInventory) {
		++OwningInventory->ReplicatedItemKey;
	}
}

#undef LOCTEXT_NAMESPACE