// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/Item.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

//Called when the inventory is changed and the UI needs an update. Optional UpdatedItem param for if an item changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

UENUM(BlueprintType)
enum class EItemAddResult: uint8 {
	IAR_NoItemsAdded UMETA(DisplayName = "No items added."),
	IAR_SomeItemsAdded UMETA(DisplayName = "Some items added."),
	IAR_AllItemsAdded UMETA(DisplayName = "All items added.")
};

USTRUCT(BlueprintType)
struct FItemAddResult {
	GENERATED_BODY()

	FItemAddResult() {};
	FItemAddResult(int32 InItemQuantity) : AmountToGive(InItemQuantity), ActualAmountGiven(0) {};
	FItemAddResult(int32 InItemQuantity, int32 InQuantityAdded) : AmountToGive(InItemQuantity), ActualAmountGiven(InQuantityAdded) {};

	UPROPERTY(BlueprintReadOnly, Category="Item Add Result")
	int32 AmountToGive;

	UPROPERTY(BlueprintReadOnly, Category="Item Add Result")
	int32 ActualAmountGiven;

	UPROPERTY(BlueprintReadOnly, Category="Item Add Result")
	EItemAddResult Result;

	UPROPERTY(BlueprintReadOnly, Category="Item Add Result")
	FText ErrorText;

	static FItemAddResult AddedNone(const int32 InItemQuantity, const FText& ErrorText) {
		FItemAddResult AddedNoneResult(InItemQuantity);

		AddedNoneResult.Result = EItemAddResult::IAR_NoItemsAdded;
		AddedNoneResult.ErrorText = ErrorText;

		return AddedNoneResult;
	}

	static FItemAddResult AddedSome(const int32 InItemQuantity, const int32 ActualAmountGiven, const FText& ErrorText) {
		FItemAddResult AddedSomeResult(InItemQuantity, ActualAmountGiven);

		AddedSomeResult.Result = EItemAddResult::IAR_NoItemsAdded;
		AddedSomeResult.ErrorText = ErrorText;

		return AddedSomeResult;
	}

	static FItemAddResult AddedAll(const int32 InItemQuantity) {
		FItemAddResult AddedAllResult(InItemQuantity, InItemQuantity);

		AddedAllResult.Result = EItemAddResult::IAR_AllItemsAdded;

		return AddedAllResult;
	}
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SURVIVALGAME_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UItem;

public:	
	// Sets default values for this component's properties
	UInventoryComponent();
	
	/**
	 * Add item to the inventory
	 * @param ErrorText the text to dispaly if the item couldnt be added to the inventory
	 * @return the amount of the item that was added to the inventory
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	FItemAddResult TryAddItem(class UItem* Item);

	/** Add item to the inventory using item class instead of item instance */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	FItemAddResult TryAddItemFromClass(TSubclassOf<class UItem> ItemClass, const int32 Quantity);

	/** Take some quantity away from the item and remove it from the inventory when quantity reaches zero
	Useful for things like eat food or use ammo */
	int32 ConsumeItem(class UItem* Item);
	int32 ConsumeItem(class UItem* Item, const int32 Quantity);

	/** Remove item from inventory */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(class UItem* Item);

	/** Return true if we have given amount of item */
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool HasItem(TSubclassOf<class UItem> ItemClass, const int32 Quantity = 1) const;

	/** Return the first item with same class as given item */
	UFUNCTION(BlueprintPure, Category="Inventory")
	UItem* FindItem(class UItem* Item) const;

	/** Return the first item with same class as ItemClass */
	UFUNCTION(BlueprintPure, Category="Inventory")
	UItem* FindItemByClass(TSubclassOf<class UItem> ItemClass) const;
	
	/** Get all inventory items that are a child of ItemClass */
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<UItem*> FindItemsByClass(TSubclassOf<class UItem> ItemClass) const;

	UFUNCTION(BlueprintPure, Category="Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetWeightCapacity(const float NewWeightCapacity);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetCapacity(const int32 NewCapacity);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE float GetWeightCapacity() const {return WeightCapacity;}

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE TArray<class UItem*> GetItems() const { return Items; }

	//Calling this on server will tell client to excute 
	UFUNCTION(Client, Reliable)
	void ClientRefreshInventory();

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

protected:

	/** Max weight the inventory can hold */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	float WeightCapacity;

	/** Max number of items in the inventory can hold */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin=0, ClampMax=200))
	int32 Capacity;

	UPROPERTY(ReplicatedUsing=OnRep_Items, VisibleAnywhere, Category="Inventory")
	TArray<class UItem*> Items;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

private:
	/** Instead of calling Items.Add() directly, use this to handle replication and ownership */
	UItem* AddItem(class UItem* Item);

	UFUNCTION()
	void OnRep_Items();
		
	UPROPERTY()
	int32 ReplicatedItemKey;

	/** Internal implementation of tryadditem(). Non-BP exposed. Do not call this directly */
	FItemAddResult TryAddItem_Internal(class UItem* Item);
};
