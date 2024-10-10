// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SurvivalCharacter.generated.h"

USTRUCT()
struct FinteractionData {
	GENERATED_BODY()

	FinteractionData() {
		ViewedInteractionComponent = nullptr;
		LastInteractionCheckTime = 0.f;
		bInteractHeld = false;
	}
	/** Current interactable component player's looking */
	UPROPERTY()
	class UInteractionComponent* ViewedInteractionComponent;

	/** last interaction checked time. for not checking interaction every frame */
	UPROPERTY()
	float LastInteractionCheckTime;

	/** is player holding the interaction key */
	UPROPERTY()
	bool bInteractHeld;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipppedItemsChanged, const EEquippableSlot, Slot, const UEquippableItem*, Item);

UCLASS()
class SURVIVALGAME_API ASurvivalCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASurvivalCharacter();

	//The mesh to have equippped if we don't have an item equipped - ie the bare skin meshes
	UPROPERTY(BlueprintReadOnly, Category="Mesh")
	TMap<EEquippableSlot, USkeletalMesh*> NakedMeshes;

	//The player's body meshes
	UPROPERTY(BlueprintReadOnly, Category="Mesh")
	TMap<EEquippableSlot, USkeletalMeshComponent*> PlayerMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UInventoryComponent* PlayerInventory;

	/** Interaction component used to allow other players to loot us when we died */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UInteractionComponent* LootPlayerInteraction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera",meta=(AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArmComponent;

	UPROPERTY(EditAnywhere, Category = "Components")
	class UCameraComponent* PlayerCameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* HelmetMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* ChestMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* LegsMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* FeetMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* VestMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* HandsMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	class USkeletalMeshComponent* BackpackMesh;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Restart() override;

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual void SetActorHiddenInGame(bool bNewHidden) override;

	void MoveForward(float Val);
	void MoveRight(float Val);

	void LookUp(float Val);
	void Turn(float Val);

	void StartCrouching();
	void StopCrouching();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool CanAim() const;

	void StartAiming();
	void StopAiming();

	void SetAiming(const bool bNewAiming);
	 
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(const bool bNewAiming);

	UPROPERTY(Transient, Replicated)
	bool bIsAiming;

	UPROPERTY(Transient, BlueprintReadWrite)
	float AimFOV;

	UPROPERTY(Transient, BlueprintReadWrite)
	float NonAimFOV;

public:
	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float SprintSpeed;

	UPROPERTY()
	float WalkSpeed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Movement")
	bool bSprinting;

	bool CanSprint() const;
	void StartSprinting();
	void StopSprinting();

	void SetSprinting(const bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(const bool bNewSprinting);


	void StartReload();

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsAlive() const {return Killer == nullptr;}

	UFUNCTION(BlueprintPure, Category="Weapons")
	FORCEINLINE bool IsAiming() const {return bIsAiming;}
	
	/** Is character interacting with item with interaction component? */
	bool IsInteracting() const;
	/** Get time to finish current interaction with focused component */
	float GetRemainingInteractTime() const;

protected:
	/** how often in seconds to to check interactable object. Zero to check every tick */
	UPROPERTY(EditDefaultsOnly,Category="Interaction")
	float InteractionCheckFrequency;
	/** how far we'll cast ray to check interactables when tracing from player */
	UPROPERTY(EditDefaultsOnly,Category="Interaction")
	float InteractionCheckDistance;

	void PerformInteractionCheck();

	void CouldntFindInteractable();
	void FoundNewInteractable(UInteractionComponent* Interactable);

	void BeginInteract();
	void EndInteract();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBeginInteract();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEndInteract();

	void Interact();

	/** Information about current state of player interaction */
	UPROPERTY()
	FinteractionData InteractionData;

	/** Get Current character's interaction component */
	FORCEINLINE class UInteractionComponent* GetInteractable() const { return InteractionData.ViewedInteractionComponent; }
	FTimerHandle TimerHandle_Interact;


public:
	/** [Server] Use an item from our inventory */
	UFUNCTION(BlueprintCallable, Category = "Items")
	void UseItem(class UItem* Item);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUseItem(class UItem* Item);

	/** [Server] Drop an item */
	UFUNCTION(BlueprintCallable, Category = "Items")
	void DropItem(class UItem* Item, const int32 Quantity);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDropItem(class UItem* Item, const int32 Quantity);

	/** Pickups use blueprint base class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "Items")
	TSubclassOf<class APickup> PickupClass;

protected:
			// Allows for efficient acccess of equipped items
	UPROPERTY(VisibleAnywhere, Category="Items")
	TMap<EEquippableSlot,UEquippableItem*> EquippedItems;

public:

	bool EquipItem(class UEquippableItem* Item);
	bool UnEquipItem(class UEquippableItem* Item);

	void EquipClothing(class UClothingItem* Clothing);
	void UnEquipClothing(const EEquippableSlot Slot);

	void EquipWeapon(class UWeaponItem* Weapon);
	void UnEquipWeapon();

	UPROPERTY(BlueprintAssignable, Category="Items")
	FOnEquipppedItemsChanged OnEquipppedItemsChanged;

	UFUNCTION(BlueprintPure)
	class USkeletalMeshComponent* GetSlotSkeletalMeshComponent(const EEquippableSlot Slot);

	UFUNCTION(BlueprintPure)
	FORCEINLINE TMap<EEquippableSlot, UEquippableItem*> GetEquippedItems() const {return EquippedItems;}

	UFUNCTION(BlueprintPure)
	FORCEINLINE class AWeapon* GetEquippedWeapon() const {return EquippedWeapon;}

	UFUNCTION(BlueprintCallable)
	void SetLootSource(class UInventoryComponent* NewLootSource);

	UFUNCTION(BlueprintPure, Category="Looting")
	bool IsLooting() const;

	UFUNCTION(BlueprintCallable, Category = "Looting")
	void LootItem(class UItem* ItemToGive);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerLootItem(class UItem* ItemToLoot);

protected:
	// Begin being looted by a player
	UFUNCTION()
	void BeginLootingPlayer(class ASurvivalCharacter* Character);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
	void ServerSetLootSource(class UInventoryComponent* NewLootSource);

	UFUNCTION()
	void OnLootSourceOwnerDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_LootSource();

	/** The inventory that we're currently looting from. Basically inventory of dead player, chest, etc. */
	UPROPERTY(ReplicatedUsing=OnRep_LootSource, BlueprintReadOnly)
	UInventoryComponent* LootSource;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	float MaxHealth;

public:
	float ModifyHealth(const float Delta);

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION(BlueprintImplementableEvent)
	void OnHealthModified(const float HealthDelta);

protected:

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_EquippedWeapon)
	class AWeapon* EquippedWeapon;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	void StartFire();
	void StopFire();

	void BeginMeleeAttack();

	UFUNCTION(Server, Reliable)
	void ServerProcessMeleeHit(const FHitResult& MeleeHit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMeleeFX();

	UPROPERTY()
	float LastMeleeAttackTime;

	UPROPERTY(EditDefaultsOnly, Category="Melee")
	float MeleeAttackDistance;

	UPROPERTY(EditDefaultsOnly, Category="Melee")
	float MeleeAttackDamage;


	UPROPERTY(EditDefaultsOnly, Category="Melee")
	class UAnimMontage* MeleeAttackMontage;

	//Called when killed by the player, or killed by something else like the environment
	void Suicide(struct FDamageEvent const& DamageEvent, const AActor* DamageCauser);
	void KilledByPlayer(struct FDamageEvent const& DamageEvent, ASurvivalCharacter* Character, const AActor* DamageCauser);

	UPROPERTY(ReplicatedUsing = OnRep_Killer)
	class ASurvivalCharacter* Killer;

	UFUNCTION()
	void OnRep_Killer();

	UFUNCTION(BlueprintImplementableEvent)
	void OnDeath();


	UFUNCTION(Server, Reliable)
	void ServerUseThrowable();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayThrowableTossFX(class UAnimMontage* ThrowMontage);

	class UThrowableItem* GetThrowable() const;

	void UseThrowable();
	void SpawnThrowable();
	bool CanUseThrowable() const;
};


