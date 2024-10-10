// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"

#define LOCTEXT_NAMESPACE "Inventory"

UInventoryComponent::UInventoryComponent()
{
	// other player need to see our inventory for looting or etc..
	SetIsReplicated(true);
	
}


FItemAddResult UInventoryComponent::TryAddItem(class UItem* Item)
{
	return TryAddItem_Internal(Item);
}

FItemAddResult UInventoryComponent::TryAddItemFromClass(TSubclassOf<class UItem> ItemClass, const int32 Quantity)
{
	UItem* Item = NewObject<UItem>(GetOwner(), ItemClass);
	Item->SetQuantity(Quantity);
	return TryAddItem_Internal(Item);
}

int32 UInventoryComponent::ConsumeItem(class UItem* Item)
{
	if (Item)
		ConsumeItem(Item, Item->GetQuantity());
	return 0;
}

int32 UInventoryComponent::ConsumeItem(class UItem* Item, const int32 Quantity)
{
	if (GetOwner() && GetOwner()->HasAuthority() && Item) {
		const int32 RemoveQuantity = FMath::Min(Quantity, Item->GetQuantity());

		//We shouldn't have negative amount of item after consume
		ensure(!(Item->GetQuantity() - RemoveQuantity < 0));
		
		Item->SetQuantity(Item->GetQuantity() - RemoveQuantity);

		//Used up all the item, remove it from the inventory
		if (Item->GetQuantity() <= 0) {
			RemoveItem(Item);
		}
		else {
			ClientRefreshInventory();
		}

		return RemoveQuantity;
	}

	return 0;
}

bool UInventoryComponent::RemoveItem(class UItem* Item)
{
	if (GetOwner() && GetOwner()->HasAuthority() && Item) {
		Items.RemoveSingle(Item);
		ReplicatedItemKey++;
		return true;
	}
	return false;
}

bool UInventoryComponent::HasItem(TSubclassOf<class UItem> ItemClass, const int32 Quantity /*= 1*/) const
{
	if (UItem* ItemToFind = FindItemByClass(ItemClass)) {
		return ItemToFind->GetQuantity() >= Quantity;
	}
	return false;
}

UItem* UInventoryComponent::FindItem(class UItem* Item) const
{
	if (Item) {
		for (auto& InvItem : Items) {
			if (InvItem && InvItem->GetClass() == Item->GetClass()) {
				return InvItem;
			}
		}
	}
	return nullptr;
}

UItem* UInventoryComponent::FindItemByClass(TSubclassOf<class UItem> ItemClass) const
{
	for (auto& InvItem : Items) {
		if (InvItem && InvItem->GetClass() == ItemClass) {
			return InvItem;
		}
	}
	return nullptr;
}

TArray<UItem*> UInventoryComponent::FindItemsByClass(TSubclassOf<class UItem> ItemClass) const
{
	TArray<UItem*> ItemsOfClass;

	for (auto& InvItem : Items) {
		if (InvItem && InvItem->GetClass()->IsChildOf(ItemClass)) {
			ItemsOfClass.Add(InvItem);
		}
	}

	return ItemsOfClass;
}

float UInventoryComponent::GetCurrentWeight() const
{
	float Weight = 0.f;
	for (auto& Item : Items) {
		if (Item){
			Weight += Item->GetStackWeight();
		}
	}
	return Weight;
}

void UInventoryComponent::SetWeightCapacity(const float NewWeightCapacity)
{
	WeightCapacity = NewWeightCapacity;
	OnInventoryUpdated.Broadcast();
}

void UInventoryComponent::SetCapacity(const int32 NewCapacity)
{
	Capacity = NewCapacity;
	OnInventoryUpdated.Broadcast();
}

void UInventoryComponent::ClientRefreshInventory_Implementation()
{
	OnInventoryUpdated.Broadcast();
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// need unrealnetwork header
	DOREPLIFETIME(UInventoryComponent, Items);
}

bool UInventoryComponent::ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	// typically anything that's not uobject only need dOREPLIFETIME along with UPROPERTY(Replicated)

	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	if (Channel->KeyNeedsToReplicate(0, ReplicatedItemKey)) {
		for (auto& Item : Items) {
			if (Channel->KeyNeedsToReplicate(Item->GetUniqueID(), Item->RepKey)) {
				bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
			}
		}
	}

	return bWroteSomething;
	
}

UItem* UInventoryComponent::AddItem(class UItem* Item)
{
	//if owner on server
	if (GetOwner() && GetOwner()->HasAuthority()) {
		//duplicate object by setting owner to this
		UItem* NewItem = NewObject<UItem>(GetOwner(), Item->GetClass());
		NewItem->World = GetWorld();
		NewItem->SetQuantity(Item->GetQuantity());
		NewItem->OwningInventory = this;
		NewItem->AddedToInventory(this);
		Items.Add(NewItem);
		NewItem->MarkDirtyForReplication();

		return NewItem;
	}

	//return null if on client
	return nullptr;
}

void UInventoryComponent::OnRep_Items()
{
	OnInventoryUpdated.Broadcast();
	for (auto& Item : Items) { //if there's too many item, this could be inefficient
		if (Item && !Item->World) {
			Item->World = GetWorld();
		}
	}
}

FItemAddResult UInventoryComponent::TryAddItem_Internal(class UItem* Item)
{
	if (GetOwner() && GetOwner()->HasAuthority() && Item) {
		const int32 AddAmount = Item->GetQuantity();

		if (Items.Num() + 1 > GetCapacity()) {
			return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryCapacityFullText", "Couldn't add item to Inventory. Inventory is FULL"));
		}

		if (!FMath::IsNearlyZero(Item->Weight)) {
			if (GetCurrentWeight() + Item->Weight > GetWeightCapacity()) {
				return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryTooMuchWeightText", "Couldn't add item to Inventory. Carrying TOO much weight"));
			}
		}

		if (Item->bStackable) {
			//Somehow item quantity went over max stack. Shouldn't happen
			ensure(Item->GetQuantity() <= Item->MaxStackSize);

			//if we already have item, just increase it's quantity
			if (UItem* ExistingItem = FindItem(Item)) {
				if (ExistingItem->GetQuantity() < ExistingItem->MaxStackSize) {
					//Find out how much of the item to add
					const int32 CapacityMaxAddAmount = ExistingItem->MaxStackSize - ExistingItem->GetQuantity();
					int32 ActualAddAmount = FMath::Min(AddAmount, CapacityMaxAddAmount);

					FText ErrorText = LOCTEXT("InventoryErrorText", "Couldn't add all of the item to your inventory");

					//Adjust based on how much weight we can carry
					if (!FMath::IsNearlyZero(Item->Weight)) {
						//Find the max amount of the item we could take due to weight
						const int32 WeightMaxAddAmount = FMath::FloorToInt((WeightCapacity - GetCurrentWeight()) / Item->Weight);
						ActualAddAmount = FMath::Min(ActualAddAmount, WeightMaxAddAmount);
						if (ActualAddAmount < AddAmount) {
							ErrorText = FText::Format(LOCTEXT("InventoryTooMuchWeightText", "Couldn't add entire stack of {ItemName} to Inventory."),Item->ItemDisplayName);
						}
					}
					else if (ActualAddAmount < AddAmount) {
						//if the weight none and we cna't take it, there was a capacity issue
						ErrorText = FText::Format(LOCTEXT("InventoryCapacityFullText", "Couldn't add entire stack of {ItemName} to Inventory. Inventory was full."), Item->ItemDisplayName);
					}

					if (ActualAddAmount <= 0) {
						return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryErrorText", "Couldn't add item to Inventory."));
					}

					ExistingItem->SetQuantity(ExistingItem->GetQuantity() + ActualAddAmount);

					//check if we somehow get more of the item than the max stack size
					ensure(ExistingItem->GetQuantity() <= ExistingItem->MaxStackSize);

					if (ActualAddAmount < AddAmount) {
						return FItemAddResult::AddedSome(AddAmount, ActualAddAmount, ErrorText);
					}
					else {
						return FItemAddResult::AddedAll(AddAmount);
					}
				}
				//we already have max size
				else {
					return FItemAddResult::AddedNone(AddAmount, FText::Format(LOCTEXT("InventoryFullStackText", "Couldn't add {ItemName}. You already have a full stack of this item"), Item->ItemDisplayName));
				}
			}
			//we don't have any of this item
			else {
				AddItem(Item);
				return FItemAddResult::AddedAll(Item->Quantity);
			}
		}
		//item's not stackable
		else {
			ensure(Item->GetQuantity() == 1);
			AddItem(Item);
			return FItemAddResult::AddedAll(Item->Quantity);
		}
	}
	//AddedItem should never be called on a client
	check(false);
	return FItemAddResult::AddedNone(-1, LOCTEXT("ErrorMessage", ""));
	
}

#undef LOCTEXT_NAMESPACE