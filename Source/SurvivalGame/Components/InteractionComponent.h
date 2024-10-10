// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "InteractionComponent.generated.h"

class ASurvivalCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginInteract, ASurvivalCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEndInteract, ASurvivalCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginFocus, ASurvivalCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEndFocus, ASurvivalCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteract, ASurvivalCharacter*, Character);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SURVIVALGAME_API UInteractionComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	UInteractionComponent();

	/** Time player must hold interaction key to interact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractionTime;
	/** Max distance of player interaction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractionDistance;
	/** Name text of widget when looking at interactable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractableNameText;
	/** Describes how interaction works */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractableActionText;
	/** Wheter we allow multiple player to interact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bAllowMultipleInteractors;
	
	/** [local+server] Called when player presses interact key while focusing on interactable */
	UPROPERTY(EditDefaultsOnly,BlueprintAssignable)
	FOnBeginInteract OnBeginInteract;
	/** [local+server] Called when player releases interact key, stop looking at interactable, or gets too far after starting interaction*/
	UPROPERTY(EditDefaultsOnly,BlueprintAssignable)
	FOnEndInteract OnEndInteract;
	/** [local+server] Called when player starts focusing on interactable */
	UPROPERTY(EditDefaultsOnly,BlueprintAssignable)
	FOnBeginFocus OnBeginFocus;
	/** [local+server] Called when player focus leaves interactable */
	UPROPERTY(EditDefaultsOnly,BlueprintAssignable)
	FOnEndFocus OnEndFocus;
	/** [local+server] Called when player interacted with item for assigned duration */
	UPROPERTY(EditDefaultsOnly,BlueprintAssignable)
	FOnInteract OnInteract;

	/** Refresh interaction widget and its custom widgets */
	void RefreshWidget();

	/** Called on client when player interaction check trace begin */
	void BeginFocus(ASurvivalCharacter* Character);
	/** Called on client when player interaction check trace end */
	void EndFocus(ASurvivalCharacter* Character);

	/** Called on client when press interact key is pressed */
	void BeginInteract(ASurvivalCharacter* Character);
	/** Called on client when press interact key is released */
	void EndInteract(ASurvivalCharacter* Character);

	void Interact(ASurvivalCharacter* Character);

	/**
	 * Return percentage of interaction by 0-1 value.
	 * On server this is first interactor's percentage.
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionPercentage();

protected:
	/** called on game start */
	virtual void Deactivate() override;

	bool CanInteract(ASurvivalCharacter* Character) const;

	/** Save all interactors interacting with this component in server. Client will only hold local player. */
	UPROPERTY()
	TArray<ASurvivalCharacter*> Interactors;
public:
	/** Change name of interactable. Also refreshes interaction widget. */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetInteractableNameText(const FText& NewNameText);
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetInteractableActionText(const FText& NewActionText);

};
