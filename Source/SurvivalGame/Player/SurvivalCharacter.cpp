// Fill out your copyright notice in the Description page of Project Settings.

#include "SurvivalCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapons/MeleeDamage.h"
#include "Net/UnrealNetwork.h"
#include "World/Pickup.h"
#include "Items/EquippableItem.h"
#include "Items/ClothingItem.h"
#include "Items/WeaponItem.h"
#include "Items/ThrowableItem.h"
#include "Materials/MaterialInstance.h"
#include "Player/SurvivalPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivalGame.h"
#include "Weapons/Weapon.h"
#include "Weapons/ThrowableWeapon.h"


#define LOCTEXT_NAMESPACE "SurvivalCharacter"

static FName NAME_AimDownSightsSocket("ADSSocket");

// Sets default values
ASurvivalCharacter::ASurvivalCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(GetMesh(), FName("CameraSocket"));
	SpringArmComponent->TargetArmLength = 0.f; // when alive, arm length is 0, after death this will increase and zoom out

	PlayerCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	PlayerCameraComponent->SetupAttachment(SpringArmComponent);
	PlayerCameraComponent->bUsePawnControlRotation = true;
	 
	HelmetMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Helmet, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HelmetMesh")));
	ChestMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Chest, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh")));
	LegsMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Legs, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh")));
	FeetMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Feet, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FeetMesh")));
	VestMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Vest, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VestMesh")));
	HandsMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Hands, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandsMesh")));
	BackpackMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Backpack, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BackpackMesh")));

	//Tell all the body meshes to use the head mesh for animation
	for (auto& PlayerMesh : PlayerMeshes) {
		USkeletalMeshComponent* MeshComponent = PlayerMesh.Value;
		MeshComponent->SetupAttachment(GetMesh());
		MeshComponent->SetMasterPoseComponent(GetMesh());
	}

	PlayerMeshes.Add(EEquippableSlot::EIS_Head, GetMesh());

	PlayerInventory = CreateDefaultSubobject<UInventoryComponent>("PlayerInventory");
	PlayerInventory-> SetCapacity(20);
	PlayerInventory->SetWeightCapacity(80.f);

	LootPlayerInteraction = CreateDefaultSubobject<UInteractionComponent>("PlayerInteraction");
	LootPlayerInteraction->InteractableActionText = LOCTEXT("LootPlayerText", "Loot");
	LootPlayerInteraction->InteractableNameText = LOCTEXT("LootPlayerName", "Player");
	LootPlayerInteraction->SetActive(false, true); //deactivate as default
	LootPlayerInteraction->bAutoActivate = false;

	InteractionCheckFrequency = 0.f;
	InteractionCheckDistance = 1000.f;

	GetMesh()->SetOwnerNoSee(true); 
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	MaxHealth = 100.f;
	Health = MaxHealth;

	SprintSpeed = GetCharacterMovement()->MaxWalkSpeed * 1.3f;
	WalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	bIsAiming = false;
	AimFOV = 70.f;
	NonAimFOV = 100.f;
}

bool ASurvivalCharacter::EquipItem(class UEquippableItem* Item)
{
	EquippedItems.Add(Item->Slot, Item);
	OnEquipppedItemsChanged.Broadcast(Item->Slot, Item);
	return true;
}

bool ASurvivalCharacter::UnEquipItem(class UEquippableItem* Item)
{
	if (Item && EquippedItems.Contains(Item->Slot)) {
		if(Item == *EquippedItems.Find(Item->Slot)) {
			EquippedItems.Remove(Item->Slot);
			OnEquipppedItemsChanged.Broadcast(Item->Slot, nullptr);
			return true;
		}
	}
	return false;
}

void ASurvivalCharacter::EquipClothing(class UClothingItem* Clothing)
{
	if (USkeletalMeshComponent* ClothingMesh = *PlayerMeshes.Find(Clothing->Slot)) {
		ClothingMesh->SetSkeletalMesh(Clothing->Mesh);
		ClothingMesh->SetMaterial(ClothingMesh->GetMaterials().Num() - 1, Clothing->MaterialInstance);
	}
}

void ASurvivalCharacter::UnEquipClothing(const EEquippableSlot Slot)
{
	if (USkeletalMeshComponent* EquippableMesh = *PlayerMeshes.Find(Slot)) {
		//find naked body mesh and set mesh and material
		if (USkeletalMesh* BodyMesh = *NakedMeshes.Find(Slot)) {
			EquippableMesh->SetSkeletalMesh(BodyMesh);

			//Put the material back on the body mesh(since clothing materials are on it)
			for (int32 i = 0; i < BodyMesh->Materials.Num(); ++i) {
				if (BodyMesh->Materials.IsValidIndex(i)) {
					EquippableMesh->SetMaterial(i, BodyMesh->Materials[i].MaterialInterface);
				}
			}
		}
		else {
			//for some slots that doens't have naked mesh ex)backpack
			EquippableMesh->SetSkeletalMesh(nullptr);
		}
	}
}

void ASurvivalCharacter::EquipWeapon(class UWeaponItem* WeaponItem)
{
	if (WeaponItem && WeaponItem->WeaponClass && HasAuthority()) {
		if (EquippedWeapon)
			UnEquipWeapon();

		FActorSpawnParameters spawnparam;
		spawnparam.bNoFail = true;
		spawnparam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		spawnparam.Owner = spawnparam.Instigator = this;

		if (AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponItem->WeaponClass, spawnparam)) {
			Weapon->Item = WeaponItem;
			EquippedWeapon = Weapon;
			OnRep_EquippedWeapon();

			Weapon->OnEquip();
		}
	}
}

void ASurvivalCharacter::UnEquipWeapon()
{
	if (HasAuthority() && EquippedWeapon) {
		EquippedWeapon->OnUnEquip();
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
		OnRep_EquippedWeapon();

	}
}

class USkeletalMeshComponent* ASurvivalCharacter::GetSlotSkeletalMeshComponent(const EEquippableSlot Slot)
{
	if (PlayerMeshes.Contains(Slot)) {
		return *PlayerMeshes.Find(Slot);
	}
	return nullptr;
}

void ASurvivalCharacter::SetLootSource(class UInventoryComponent* NewLootSource)
{
	// remove dead body after 2min
	// if the item we're looting gets destoryed, we need to tell the client to remove their loot screen
	if (NewLootSource && NewLootSource->GetOwner()) {
		NewLootSource->GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ASurvivalCharacter::OnLootSourceOwnerDestroyed);
	}


	if (HasAuthority()) {
		if (NewLootSource) {
			//Looting a player keeps thir body alive for extra 2min to provide enough time to loot
			if (ASurvivalCharacter* Character = Cast<ASurvivalCharacter>(NewLootSource->GetOwner())) {
				Character->SetLifeSpan(120.f);
			}
		}
		LootSource = NewLootSource;
	}
	else {
		ServerSetLootSource(NewLootSource);
	}
}

bool ASurvivalCharacter::IsLooting() const
{
	return LootSource != nullptr;
}

void ASurvivalCharacter::LootItem(class UItem* ItemToGive)
{
	if (HasAuthority()) {
		if (PlayerInventory && LootSource && ItemToGive && LootSource->HasItem(ItemToGive->GetClass(), ItemToGive->GetQuantity())) {
			const FItemAddResult AddResult = PlayerInventory->TryAddItem(ItemToGive);

			if (AddResult.ActualAmountGiven > 0) {
				LootSource->ConsumeItem(ItemToGive, AddResult.ActualAmountGiven);
			}
			else {
				if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController())) {
					PC->ClientShowNotification(AddResult.ErrorText);
				}
			}
		}
	}
	else {
		ServerLootItem(ItemToGive);
	}
	
}

void ASurvivalCharacter::ServerLootItem_Implementation(class UItem* ItemToLoot)
{
	LootItem(ItemToLoot);
}

bool ASurvivalCharacter::ServerLootItem_Validate(class UItem* ItemToLoot)
{
	return true;
}

void ASurvivalCharacter::BeginLootingPlayer(class ASurvivalCharacter* Character)
{
	if (Character) {
		Character->SetLootSource(PlayerInventory);
	}
}

void ASurvivalCharacter::ServerSetLootSource_Implementation(class UInventoryComponent* NewLootSource)
{
	SetLootSource(NewLootSource);
}

bool ASurvivalCharacter::ServerSetLootSource_Validate(class UInventoryComponent* NewLootSource)
{
	return true;
}

void ASurvivalCharacter::OnLootSourceOwnerDestroyed(AActor* DestroyedActor)
{
	// Remove loot source
	if (!HasAuthority() && LootSource && DestroyedActor == LootSource->GetOwner()) {
		ServerSetLootSource(nullptr);
	}
}

void ASurvivalCharacter::OnRep_LootSource()
{
	if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController())) {
		if (PC->IsLocalController()) {
			if (LootSource) {
				PC->ShowLootMenu(LootSource);
			}
			else {
				PC->HideLootMenu();
			}
		}
	}
}

float ASurvivalCharacter::ModifyHealth(const float Delta)
{
	const float OldHealth = Health;
	Health = FMath::Clamp<float>(Health + Delta, 0.f, MaxHealth);

	//how much it is modified
	return Health - OldHealth;
}

void ASurvivalCharacter::OnRep_Health(float OldHealth)
{
	OnHealthModified(Health - OldHealth);
}

void ASurvivalCharacter::OnRep_EquippedWeapon()
{
	if (EquippedWeapon) { //other client calls this
		EquippedWeapon->OnEquip();
	} //unequipping destorys so no need for that
}

void ASurvivalCharacter::StartFire()
{
	if (EquippedWeapon) {
		EquippedWeapon->StartFire();
	}
	else {
		BeginMeleeAttack();
	}
	
}

void ASurvivalCharacter::StopFire()
{
	if (EquippedWeapon) {
		EquippedWeapon->StopFire();
	}
}

void ASurvivalCharacter::BeginMeleeAttack()
{
	if (GetWorld()->TimeSince(LastMeleeAttackTime) > MeleeAttackMontage->GetPlayLength()) { //limit punching interval to animation speed

		FHitResult Hit;
		FCollisionShape Shape = FCollisionShape::MakeSphere(15.f);
		FVector StartTrace = PlayerCameraComponent->GetComponentLocation();
		FVector EndTrace = (PlayerCameraComponent->GetComponentRotation().Vector() * MeleeAttackDistance) + StartTrace;

		FCollisionQueryParams QueryParams = FCollisionQueryParams("MeleeSweep", false, this);

		PlayAnimMontage(MeleeAttackMontage);

		if (GetWorld()->SweepSingleByChannel(Hit, StartTrace, EndTrace, FQuat(), COLLISION_WEAPON, Shape, QueryParams)) {
			UE_LOG(LogTemp, Warning, TEXT("We hit something with our punch"));

			if (ASurvivalCharacter* HitPlayer = Cast<ASurvivalCharacter>(Hit.GetActor())) {
				if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController())) {
					PC->OnHitPlayer(); //display hitmarker or something
				}
			}
		}

		ServerProcessMeleeHit(Hit);

		LastMeleeAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void ASurvivalCharacter::MulticastPlayMeleeFX_Implementation()
{
	if (!IsLocallyControlled()) {
		PlayAnimMontage(MeleeAttackMontage);
	}
}

void ASurvivalCharacter::ServerProcessMeleeHit_Implementation(const FHitResult& MeleeHit) 
{
	MulticastPlayMeleeFX(); //play anim to all client

	if (GetWorld()->TimeSince(LastMeleeAttackTime) > MeleeAttackMontage->GetPlayLength() //prevent hitting to fast
		&& (GetActorLocation()- MeleeHit.ImpactPoint).Size() <= MeleeAttackDistance ) //prevents cheating distance
	{ 
		UGameplayStatics::ApplyPointDamage(MeleeHit.GetActor(), MeleeAttackDamage, (MeleeHit.TraceStart - MeleeHit.TraceEnd).GetSafeNormal(), MeleeHit, GetController(), this, UMeleeDamage::StaticClass());

	}
	LastMeleeAttackTime = GetWorld()->GetTimeSeconds();
}


void ASurvivalCharacter::Suicide(struct FDamageEvent const& DamageEvent, const AActor* DamageCauser)
{
	Killer = this;
	OnRep_Killer();
}

void ASurvivalCharacter::KilledByPlayer(struct FDamageEvent const& DamageEvent, class ASurvivalCharacter* Character, const AActor* DamageCauser)
{
	Killer = Character;
	OnRep_Killer();
}

void ASurvivalCharacter::OnRep_Killer()
{
	SetLifeSpan(20.f); //dispose character after 20sec
	 
	//ragdoll
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	SetReplicatingMovement(false);

	LootPlayerInteraction->Activate();

	// Unequip all equippable so they can be looted
	if (HasAuthority()) {
		TArray<UEquippableItem*> Equippables;
		EquippedItems.GenerateValueArray(Equippables);

		for (auto& EquippedItem : Equippables) {
			EquippedItem->SetEquipped(false);
		}
	}

	if (IsLocallyControlled()) {
		SpringArmComponent->TargetArmLength = 500.f;
		SpringArmComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		bUseControllerRotationPitch = true; // rotate around our character

		if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController())) {
			PC->ShowDeathScreen(Killer);
		}
	}
}

void ASurvivalCharacter::ServerUseThrowable_Implementation()
{
	UseThrowable();
}

void ASurvivalCharacter::MulticastPlayThrowableTossFX_Implementation(UAnimMontage* ThrowMontage)
{
	//Server doesn't need FX and local machine we instantly played animation
	if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled()) {
		PlayAnimMontage(ThrowMontage);
	}
}

class UThrowableItem* ASurvivalCharacter::GetThrowable() const
{
	UThrowableItem* EquippedThrowable = nullptr;
	if (EquippedItems.Contains(EEquippableSlot::EIS_Throwable)) {
		EquippedThrowable = Cast<UThrowableItem>(*EquippedItems.Find(EEquippableSlot::EIS_Throwable));
	}
	return EquippedThrowable;
}

void ASurvivalCharacter::UseThrowable()
{
	if (CanUseThrowable()) {
		if (UThrowableItem* Throwable = GetThrowable()) {
			if (HasAuthority()) {
				SpawnThrowable();
				if(PlayerInventory) PlayerInventory->ConsumeItem(Throwable, 1);
			}
			else {
				if (Throwable->GetQuantity() <= 1) { //if it is last item, pre-destroy it
					EquippedItems.Remove(EEquippableSlot::EIS_Throwable);
					OnEquipppedItemsChanged.Broadcast(EEquippableSlot::EIS_Throwable, nullptr);
				}
				PlayAnimMontage(Throwable->ThrowableTossAnimation);
				ServerUseThrowable();
			}
		}
		
	}
}

void ASurvivalCharacter::SpawnThrowable()
{
	if (HasAuthority()) {
		if (UThrowableItem* CurrentThrowable = GetThrowable()) {
			if (CurrentThrowable->ThrowableClass) {
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = SpawnParams.Instigator = this;
				SpawnParams.bNoFail = true;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				FVector EyesLoc;
				FRotator EyesRot;

				GetController()->GetPlayerViewPoint(EyesLoc, EyesRot);

				//Spawn slightly in front of face to evade collision with player
				EyesLoc = (EyesRot.Vector() * 20.f) + EyesLoc;

				if (AThrowableWeapon* ThrowableWeapon = GetWorld()->SpawnActor<AThrowableWeapon>(CurrentThrowable->ThrowableClass, FTransform(EyesRot, EyesLoc))) {
					MulticastPlayThrowableTossFX(CurrentThrowable->ThrowableTossAnimation);
				}

			}
		}
	}
}

bool ASurvivalCharacter::CanUseThrowable() const
{
	return GetThrowable() != nullptr && GetThrowable()->ThrowableClass != nullptr;
}

// Called when the game starts or when spawned
void ASurvivalCharacter::BeginPlay()
{
	Super::BeginPlay();

	LootPlayerInteraction->OnInteract.AddDynamic(this, &ASurvivalCharacter::BeginLootingPlayer);

	if (APlayerState* PS = GetPlayerState()) {
		LootPlayerInteraction->SetInteractableNameText(FText::FromString(PS->GetPlayerName()));
	}

	//When the player spawns in they have no items equipped, so cache these items(that way, if a player unequips an item we can set the mesh back to naked
	for (auto& PlayerMesh : PlayerMeshes) {
		NakedMeshes.Add(PlayerMesh.Key, PlayerMesh.Value->SkeletalMesh);
	}
	
}

void ASurvivalCharacter::Restart()
{
	Super::Restart();

	if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController())) {
		PC->ShowIngameUI();
	}
}

float ASurvivalCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	const float DamageDealt = ModifyHealth(-Damage);

	if (Health <= 0.f) {
		if (ASurvivalCharacter* dmgkiller = Cast<ASurvivalCharacter>(DamageCauser->GetOwner())) {
			KilledByPlayer(DamageEvent, dmgkiller, DamageCauser);
		}
		else {
			Suicide(DamageEvent, DamageCauser);
		}
	}

	return DamageDealt;
}

void ASurvivalCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);

	if (EquippedWeapon) {
		EquippedWeapon->SetActorHiddenInGame(bNewHidden);
	}
}

void ASurvivalCharacter::MoveForward(float Val)
{
	if (Val != 0.f) {
		AddMovementInput(GetActorForwardVector(), Val);
	}
}

void ASurvivalCharacter::MoveRight(float Val)
{
	if (Val != 0.f) {
		AddMovementInput(GetActorRightVector(), Val);
	}
}

void ASurvivalCharacter::LookUp(float Val)
{
	if(Val != 0.f)
		AddControllerPitchInput(Val);
}

void ASurvivalCharacter::Turn(float Val)
{
	if (Val != 0.f)
		AddControllerYawInput(Val);
}

void ASurvivalCharacter::StartCrouching()
{
	Crouch();
}

void ASurvivalCharacter::StopCrouching()
{
	UnCrouch();
}

void ASurvivalCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivalCharacter, bSprinting);
	DOREPLIFETIME(ASurvivalCharacter, LootSource);
	DOREPLIFETIME(ASurvivalCharacter, EquippedWeapon);
	DOREPLIFETIME(ASurvivalCharacter, Killer);

	/**
	* if you want to make appearance change by remaining health, this should be replicated to everyone
	* otherwise other players can't see the changes
	*/
	DOREPLIFETIME_CONDITION(ASurvivalCharacter, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ASurvivalCharacter, bIsAiming, COND_SkipOwner); //local aiming is done instant locally

	//DOREPLIFETIME(ASurvivalCharacter, Killer);
}

bool ASurvivalCharacter::CanAim() const
{
	return EquippedWeapon != nullptr;
}

void ASurvivalCharacter::StartAiming()
{
	if (CanAim()) {
		SetAiming(true);
	}
}

void ASurvivalCharacter::StopAiming()
{
	SetAiming(false);
}

void ASurvivalCharacter::SetAiming(const bool bNewAiming)
{
	if (bNewAiming == bIsAiming || (bNewAiming && !CanAim())) {
		return;
	}
	if (!HasAuthority()) {
		ServerSetAiming(bNewAiming);
	}
	
	bIsAiming = bNewAiming;
}

void ASurvivalCharacter::ServerSetAiming_Implementation(const bool bNewAiming)
{
	SetAiming(bNewAiming);
}

void ASurvivalCharacter::CouldntFindInteractable()
{
	//lost focus on interactable. clearing timer
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_Interact)) {
		GetWorldTimerManager().ClearTimer(TimerHandle_Interact);
	}
	//if we just had interactable end focus and end interaction if key was pressed
	if (UInteractionComponent* Interactable = GetInteractable()) {
		Interactable->EndFocus(this);
		if (InteractionData.bInteractHeld) {
			EndInteract();
		}
	}

	InteractionData.ViewedInteractionComponent = nullptr;
}

void ASurvivalCharacter::FoundNewInteractable(UInteractionComponent* Interactable)
{
	//end previous interaction
	EndInteract();
	//hide old one
	if (UInteractionComponent* OldInteractable = GetInteractable()) {
		OldInteractable->EndFocus(this);
	}
	InteractionData.ViewedInteractionComponent = Interactable;
	Interactable->BeginFocus(this);
}

void ASurvivalCharacter::BeginInteract()
{
	if (!HasAuthority()) {
		ServerBeginInteract();
	}

	/**
	 * As an optimization, the server only checks that we're looking at an item once we begin interacting with it.
	 * This saves the server doing a check every tick for an iteractable Item. The exception is a non-instant interact.
	 * In this case, the server will check every tick for the duration of the interact.
	 */
	if (HasAuthority()) {
		PerformInteractionCheck();
	}


	InteractionData.bInteractHeld = true;

	if (UInteractionComponent* Interactable = GetInteractable()) {
		Interactable->BeginInteract(this);
		if (FMath::IsNearlyZero(Interactable->InteractionTime)) {
			Interact();
		}
		else {
			GetWorldTimerManager().SetTimer(TimerHandle_Interact, this, &ASurvivalCharacter::Interact, Interactable->InteractionTime, false);
		}
	}
}

void ASurvivalCharacter::EndInteract()
{
	if (!HasAuthority()) {
		ServerEndInteract();
	}

	InteractionData.bInteractHeld = false;

	GetWorldTimerManager().ClearTimer(TimerHandle_Interact);
	if (UInteractionComponent* Interactable = GetInteractable()) {
		Interactable->EndInteract(this);
	}
}

void ASurvivalCharacter::ServerBeginInteract_Implementation()
{
	BeginInteract();
}

bool ASurvivalCharacter::ServerBeginInteract_Validate()
{
	return true;
} 

void ASurvivalCharacter::ServerEndInteract_Implementation()
{
	EndInteract();
}

bool ASurvivalCharacter::ServerEndInteract_Validate()
{
	return true;
}

void ASurvivalCharacter::Interact()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Interact);
	if (UInteractionComponent* Interactable = GetInteractable()) {
		Interactable->Interact(this);
	}

}

bool ASurvivalCharacter::CanSprint() const
{
	return !IsAiming();
}

void ASurvivalCharacter::StartSprinting()
{
	SetSprinting(true);
}

void ASurvivalCharacter::StopSprinting()
{
	SetSprinting(false);
}

void ASurvivalCharacter::SetSprinting(const bool bNewSprinting)
{
	if ((bNewSprinting && !CanSprint()) || bNewSprinting == bSprinting) {
		return;
	}

	if (!HasAuthority()) {
		ServerSetSprinting(bNewSprinting);
	}
	bSprinting = bNewSprinting;

	//TODO : server authoritative network character movement
	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

void ASurvivalCharacter::ServerSetSprinting_Implementation(const bool bNewSprinting)
{
	SetSprinting(bNewSprinting);
}

void ASurvivalCharacter::StartReload()
{
	if (EquippedWeapon) {
		EquippedWeapon->StartReload();
	}
}

bool ASurvivalCharacter::IsInteracting() const
{
	return GetWorldTimerManager().IsTimerActive(TimerHandle_Interact);
}

float ASurvivalCharacter::GetRemainingInteractTime() const
{
	return GetWorldTimerManager().GetTimerRemaining(TimerHandle_Interact);
}

void ASurvivalCharacter::UseItem(class UItem* Item)
{
	//GetLocalRole() < ROLE_Authority
	if (!HasAuthority() && Item) {
		ServerUseItem(Item);
	}

	if (HasAuthority()) {
		//safety check in case client uses invalid item
		if (PlayerInventory && !PlayerInventory->FindItem(Item))  return;
	}

	//both will use
	if (Item) {
		Item->OnUse(this); //spawn particles or something
		Item->Use(this);
	}
}

void ASurvivalCharacter::ServerUseItem_Implementation(class UItem* Item)
{
	UseItem(Item);
}

bool ASurvivalCharacter::ServerUseItem_Validate(class UItem* Item)
{
	return true;
}

void ASurvivalCharacter::DropItem(class UItem* Item, const int32 Quantity)
{
	if (PlayerInventory && Item && PlayerInventory->FindItem(Item)) {
		//server will drop the item for client
		if (!HasAuthority()) {
			ServerDropItem(Item, Quantity);
			return;
		} else{
			//server
			const int32 ItemQuantity = Item->GetQuantity();
			const int32 DroppedQuantity = PlayerInventory->ConsumeItem(Item, Quantity);

			//spawn pickup
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.bNoFail = true; //always spawn
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn; // prevents item stuck in wall or smt like that

			FVector SpawnLocation = GetActorLocation();
			SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

			FTransform SpawnTransform(GetActorRotation(), SpawnLocation);

			ensure(PickupClass);

			APickup* Pickup = GetWorld()->SpawnActor<APickup>(PickupClass, SpawnTransform, SpawnParams);
			Pickup->InitializePickup(Item->GetClass(), DroppedQuantity);
		}
	}
}

void ASurvivalCharacter::ServerDropItem_Implementation(class UItem* Item, const int32 Quantity)
{
	DropItem(Item, Quantity);
}

bool ASurvivalCharacter::ServerDropItem_Validate(class UItem* Item, const int32 Quantity)
{
	return true;
}

// Called every frame
void ASurvivalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//optimization : widget needs to popup quickly on client, but server only needs to know when it tries to interact
	const bool bIsInteractingOnServer = (HasAuthority() && IsInteracting());

	//optimization : perform trace check by given frequency, not every frame
	if ((!HasAuthority() || bIsInteractingOnServer) && GetWorld()->TimeSince(InteractionData.LastInteractionCheckTime) > InteractionCheckFrequency) {
		PerformInteractionCheck();
	}

	if (IsLocallyControlled()) {
		const float DesiredFOV = IsAiming() ? AimFOV : NonAimFOV;
		PlayerCameraComponent->SetFieldOfView(FMath::FInterpTo(PlayerCameraComponent->FieldOfView, DesiredFOV, DeltaTime, 10.f));

		if (EquippedWeapon) {
			const FVector ADSLocation = EquippedWeapon->GetWeaponMesh()->GetSocketLocation(NAME_AimDownSightsSocket);
			const FVector DefaultCameraLocation = GetMesh()->GetSocketLocation(FName("CameraSocket"));

			FVector CameraLoc = bIsAiming ? ADSLocation : DefaultCameraLocation;

			const float InterpSpeed = FVector::Dist(ADSLocation, DefaultCameraLocation) / EquippedWeapon->ADSTime;
			PlayerCameraComponent->SetWorldLocation(FMath::VInterpTo(PlayerCameraComponent->GetComponentLocation(), CameraLoc, DeltaTime, InterpSpeed));
		}
	}

}

void ASurvivalCharacter::PerformInteractionCheck()
{
	if (GetController() == nullptr) {
		return;
	}

	InteractionData.LastInteractionCheckTime = GetWorld()->GetTimeSeconds();

	FVector EyesLocation;
	FRotator EyesRotation;
	GetController()->GetPlayerViewPoint(EyesLocation, EyesRotation);

	FVector TraceStart = EyesLocation;
	FVector TraceEnd = (EyesRotation.Vector() * InteractionCheckDistance) + TraceStart;
	FHitResult TraceHit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); //ignore colliding with character itself

	if (GetWorld()->LineTraceSingleByChannel(TraceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams)) {
		if (TraceHit.GetActor()) {
			//check if hit actor has interaction component
			if (UInteractionComponent* InteractionComponent = Cast<UInteractionComponent>(TraceHit.GetActor()->GetComponentByClass(UInteractionComponent::StaticClass()))) {

				float Distance = (TraceStart - TraceHit.ImpactPoint).Size();

				//check if it is interactable we're already looking
				if (InteractionComponent != GetInteractable() && Distance <= InteractionComponent->InteractionDistance) {
					FoundNewInteractable(InteractionComponent);
				}
				//was looking at the interactable but moved away from it
				else if(Distance>InteractionComponent->InteractionDistance && GetInteractable()){
					CouldntFindInteractable();
				}

				return;
			}
		}
	}

	//didn't hit anything interactable
	CouldntFindInteractable();
}

// Called to bind functionality to input
void ASurvivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ASurvivalCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ASurvivalCharacter::StopFire);

	PlayerInputComponent->BindAction("Aiming", IE_Pressed, this, &ASurvivalCharacter::StartAiming);
	PlayerInputComponent->BindAction("Aiming", IE_Released, this, &ASurvivalCharacter::StopAiming);

	PlayerInputComponent->BindAction("Throw", IE_Pressed, this, &ASurvivalCharacter::UseThrowable);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ASurvivalCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ASurvivalCharacter::StopJumping);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASurvivalCharacter::BeginInteract);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &ASurvivalCharacter::EndInteract);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ASurvivalCharacter::StartCrouching);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ASurvivalCharacter::StopCrouching);

	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ASurvivalCharacter::StartSprinting);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ASurvivalCharacter::StopSprinting);

	PlayerInputComponent->BindAxis("MoveForward", this, &ASurvivalCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASurvivalCharacter::MoveRight);

}

#undef LOCTEXT_NAMESPACE