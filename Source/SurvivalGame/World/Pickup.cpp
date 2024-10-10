// Fill out your copyright notice in the Description page of Project Settings.


#include "World/Pickup.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Player/SurvivalCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/InventoryComponent.h"
#include "Items/Item.h"

// Sets default values
APickup::APickup()
{
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>("PickupMesh");
	//pawns do not collide with this
	PickupMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	SetRootComponent(PickupMesh);

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>("PickupInteractionComponent");
	InteractionComponent->InteractionTime = 0.5f;
	InteractionComponent->InteractionDistance = 200.f;
	InteractionComponent->InteractableNameText = FText::FromString("Pickup");
	InteractionComponent->InteractableActionText = FText::FromString("Take");
	InteractionComponent->OnInteract.AddDynamic(this, &APickup::OnTakePickup);
	InteractionComponent->SetupAttachment(PickupMesh);
	
	SetReplicates(true);
}

void APickup::InitializePickup(const TSubclassOf<class UItem> ItemClass, const int32 Quantity)
{
	if (HasAuthority() && ItemClass && Quantity > 0) {
		Item = NewObject<UItem>(this, ItemClass);
		Item->SetQuantity(Quantity);

		//so that client get updated about what the item is and how it's changed
		OnRep_Item();

		Item->MarkDirtyForReplication();
	}
}

void APickup::OnRep_Item()
{
	if (Item) {
		PickupMesh->SetStaticMesh(Item->PickupMesh);
		InteractionComponent->InteractableNameText = Item->ItemDisplayName;

		//Clients bind to this delegate in order to refresh the interaction widget if item quantity changes
		Item->OnItemModified.AddDynamic(this, &APickup::OnItemModified);
	}

	//If any replicated properties on the itme are changed, we refresh the widget
	InteractionComponent->RefreshWidget();
}

void APickup::OnItemModified()
{
	if (InteractionComponent) {
		InteractionComponent->RefreshWidget();
	}
}

// Called when the game starts or when spawned
void APickup::BeginPlay()
{
	Super::BeginPlay();

	//bnetstartup false => spawned in true => placed in level
	if (HasAuthority()&& ItemTemplate && bNetStartup) {
		InitializePickup(ItemTemplate->GetClass(), ItemTemplate->GetQuantity());
	}

	/** If pickup was spawned in at runtime, ensure that it matches the rotation of the ground that it was dropped on*/
	if (!bNetStartup) {
		AlignWithGround();
	}

	if (Item) {
		//for KeyNeedstoReplicate
		Item->MarkDirtyForReplication();
	}
	
}

void APickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickup, Item);
}

bool APickup::ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	//InvComp¶û À¯»ç
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	//Keyneedstoreplicate only returns true when it is marked dirty
	if (Item && Channel->KeyNeedsToReplicate(Item->GetUniqueID(), Item->RepKey)) {
			bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
	}


	return bWroteSomething;
}
#if WITH_EDITOR
void APickup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//If a new pickup is selected in the property editor, change the mesh to reflect the new item being selected
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APickup, ItemTemplate)) {
		if (ItemTemplate) {
			PickupMesh->SetStaticMesh(ItemTemplate->PickupMesh);
		}
	}
}
#endif

void APickup::OnTakePickup(class ASurvivalCharacter* Taker)
{
	if (!Taker) {
		UE_LOG(LogTemp, Warning, TEXT("Pickup was taken but player was not valid. "));
		return;
	}
	// Not 100% sure Pending kill check is needed but should prevent player from taking a pickup another player has already tried taking
	if (HasAuthority() && !IsPendingKill() && Item) {
		UE_LOG(LogTemp, Warning, TEXT("trying to pick up. "));
		if (UInventoryComponent* PlayerInventory = Taker->PlayerInventory) {
			const FItemAddResult AddResult = PlayerInventory->TryAddItem(Item);

			//if not all item was taken, don't destory pickup. Just reduce quantity.
			if (AddResult.ActualAmountGiven < Item->GetQuantity()) {
				Item->SetQuantity(Item->GetQuantity() - AddResult.ActualAmountGiven);
				UE_LOG(LogTemp, Warning, TEXT("Not all was taken"));
			}
			else if (AddResult.ActualAmountGiven >= Item->GetQuantity()) {
				Destroy();
			}
		}
	}

}

