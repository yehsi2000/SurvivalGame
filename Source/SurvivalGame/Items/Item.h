// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Item.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemModified);

UENUM(BlueprintType)
enum class EItemRarity :uint8 {
	IR_Common UMETA(DisplayName = "Common"),
	IR_Uncommon UMETA(DisplayName = "Uncommon"),
	IR_Rare UMETA(DisplayName = "Rare"),
	IR_Epic UMETA(DisplayName = "Epic"),
	IR_Legendary UMETA(DisplayName = "Legendary"),
};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class SURVIVALGAME_API UItem : public UObject
{
	GENERATED_BODY()

protected:
	/** For networking */
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override;
	virtual class UWorld* GetWorld() const override;

#if WITH_EDITOR
	//only included in editor
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
public:
	UItem();

	UPROPERTY(Transient)
	class UWorld* World; // for spawning particles you need world

	/** Mesh to display for world item pickup */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	class UStaticMesh* PickupMesh;

	/** Thumbnail image for item tooltip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	class UTexture2D* Thumbnail;
	
	/** Display name for item in inventory */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	FText ItemDisplayName;

	/** Optional Description text for item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item", meta = (MultiLine = true))
	FText ItemDescription;

	/** Text for action corresponding to this item (Equip, Eat...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	FText UseActionText;

	/** Enum for item rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	EItemRarity Rarity;

	/** Weight of this item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item", meta = (ClampMin = 0.0))
	float Weight;

	/** Could multiple item be stacked in single inventory slot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	bool bStackable;

	/** Max size of item stack in single slot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item", meta= (ClampMin = 2, EditCondition = bStackable))
	int32 MaxStackSize;
	
	/**	The tooltip in the inventory for this item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Item")
	TSubclassOf<class UItemToolTip> ItemTooltip;

	/**
	 * Amount of item
	 * if server changes this value, client will call OnRep_Quantity automatically
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Quantity, EditAnywhere, Category="Item", meta = (UIMin =1, EditCondition = bStackable))
	int32 Quantity;

	/** InventoryComponent that owns this item */
	UPROPERTY()
	class UInventoryComponent* OwningInventory;

	UPROPERTY(BlueprintAssignable)
	FOnItemModified OnItemModified;

	/** Used to efficiently replicate inventory item */
	// shouldn't have to do this in Actor Class as it's already implemented
	// whenever we change something about this item we change this key
	// changing this value will alert server to send update to client
	UPROPERTY()
	int32 RepKey;

	UFUNCTION()
	void OnRep_Quantity();

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetQuantity(const int32 NewQuantity);

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE int32 GetQuantity() const {return Quantity;}

	UFUNCTION(BlueprintCallable, Category = "Item")
	FORCEINLINE float GetStackWeight() const {return Quantity * Weight;}

	/** For clothing. It shouldn't show in inventory when equipped but still exists */
	UFUNCTION(BlueprintPure, Category = "Item")
	virtual bool ShouldShowInInventory() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnUse(class ASurvivalCharacter* Character);

	virtual void Use(class ASurvivalCharacter* Character);
	virtual void AddedToInventory(class UInventoryComponent* Inventory);

	/** Mark object as needing replication. Must call this internally after modifying any replicated properties */
	void MarkDirtyForReplication();
};
