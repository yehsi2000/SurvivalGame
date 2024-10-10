// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/InteractionComponent.h"
#include "Player/SurvivalCharacter.h"
#include "Widgets/InteractionWidget.h"

UInteractionComponent::UInteractionComponent()
{
	SetComponentTickEnabled(false);
	InteractionTime = 0.f;
	InteractionDistance = 200.f; //2meter
	InteractableNameText = FText::FromString("Interactable Object");
	InteractableActionText = FText::FromString("Interact");
	bAllowMultipleInteractors = true;

	Space = EWidgetSpace::Screen;
	DrawSize = FIntPoint(600, 100);
	bDrawAtDesiredSize = true;

	SetActive(true);
	SetHiddenInGame(true);

}

void UInteractionComponent::RefreshWidget()
{
	if (!bHiddenInGame && GetOwner()->GetNetMode() != NM_DedicatedServer) {
		if (UInteractionWidget* InteractionWidget = Cast<UInteractionWidget>(GetUserWidgetObject())) {
			InteractionWidget->UpdateInteractionWidget(this);
		}
	}
}

void UInteractionComponent::BeginFocus(ASurvivalCharacter* Character)
{
	if (!IsActive() || !GetOwner() || !Character) {
		return;
	}

	//call binded function on this delegate
	OnBeginFocus.Broadcast(Character);

	SetHiddenInGame(false);

	//Set object outlined on client
	if (!GetOwner()->HasAuthority()){
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(UPrimitiveComponent::StaticClass(), Components);
		for (auto& visualComp : Components) {
			if (UPrimitiveComponent* prim = Cast<UPrimitiveComponent>(visualComp)) {
				prim->SetRenderCustomDepth(true);
			}
		}
	}

	RefreshWidget();
}

void UInteractionComponent::EndFocus(ASurvivalCharacter* Character)
{
	OnEndFocus.Broadcast(Character);

	SetHiddenInGame(true);

	if (!GetOwner()->HasAuthority()) {
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(UPrimitiveComponent::StaticClass(), Components);
		for (auto& visualComp : Components) {
			if (UPrimitiveComponent* prim = Cast<UPrimitiveComponent>(visualComp)) {
				prim->SetRenderCustomDepth(false);
			}
		}
	}
}

void UInteractionComponent::BeginInteract(ASurvivalCharacter* Character)
{
	if (CanInteract(Character)) {
		Interactors.AddUnique(Character);
		OnBeginInteract.Broadcast(Character);
	}
}

void UInteractionComponent::EndInteract(ASurvivalCharacter* Character)
{
	Interactors.RemoveSingle(Character);
	OnEndInteract.Broadcast(Character);
}

void UInteractionComponent::Interact(ASurvivalCharacter* Character)
{
	if (CanInteract(Character)) {
		OnInteract.Broadcast(Character);
	}
}

float UInteractionComponent::GetInteractionPercentage()
{
	if (Interactors.IsValidIndex(0)) {
		if (ASurvivalCharacter* Interactor = Interactors[0]) {
			if (Interactor && Interactor->IsInteracting()) {
				return 1.f - FMath::Abs(Interactor->GetRemainingInteractTime() / InteractionTime);
			}
		}
	}
	return 0.f;
}

void UInteractionComponent::Deactivate()
{
	Super::Deactivate();

	for (int32 i = Interactors.Num() - 1; i >= 0; --i) {
		if (ASurvivalCharacter* Interactor = Interactors[i]) {
			EndFocus(Interactor);
			EndInteract(Interactor);
		}
	}

	Interactors.Empty();
}

bool UInteractionComponent::CanInteract(ASurvivalCharacter* Character) const
{
	// More than one characters are interacting and this component doesn't allow it
	const bool bPlayerAlreadyInteracting = !bAllowMultipleInteractors && Interactors.Num() >= 1;
	// Component is active, owning actor and interacting character is avaialbe
	return !bPlayerAlreadyInteracting && IsActive() && GetOwner() != nullptr && Character != nullptr;
}

void UInteractionComponent::SetInteractableNameText(const FText& NewNameText)
{
	InteractableNameText = NewNameText;
	RefreshWidget();
}

void UInteractionComponent::SetInteractableActionText(const FText& NewActionText)
{
	InteractableActionText = NewActionText;
	RefreshWidget();
}
