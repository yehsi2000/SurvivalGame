// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALGAME_API ASurvivalPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASurvivalPlayerController();

	virtual void SetupInputComponent();

	//Call this instead of show notification if on the server
	UFUNCTION(Client, Reliable, BlueprintCallable)
	void ClientShowNotification(const FText& Message);
	UFUNCTION(BlueprintImplementableEvent)
	void ShowNotification(const FText& Message);
	UFUNCTION(BlueprintImplementableEvent)
	void ShowDeathScreen(class ASurvivalCharacter* Killer);
	UFUNCTION(BlueprintImplementableEvent)
	void ShowLootMenu(const class UInventoryComponent* LootSource);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void HideLootMenu();
	UFUNCTION(BlueprintImplementableEvent)
	void OnHitPlayer();

	UFUNCTION(BlueprintCallable, Category = "Survival Controller")
	void Respawn();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRespawn();


	//will be implemented in blueprint
	UFUNCTION(BlueprintImplementableEvent)
	void ShowIngameUI();

public:

	void ApplyRecoil(const FVector2D& RecoilAmount, const float RecoilSpeed, const float RecoilResetSpeed, TSubclassOf<class UCameraShakeBase> Shake = nullptr);
	
	UPROPERTY(VisibleAnywhere, Category="Recoil")
	FVector2D RecoilBumpAmount;

	UPROPERTY(VisibleAnywhere, Category="Recoil")
	FVector2D RecoilResetAmount;

	UPROPERTY(VisibleAnywhere, Category="Recoil")
	float CurrentRecoilSpeed;

	UPROPERTY(VisibleAnywhere, Category="Recoil")
	float CurrentRecoilResetSpeed;

	UPROPERTY(VisibleAnywhere, Category="Recoil")
	float LastRecoilTime;

	void Turn(float Rate);
	void LookUp(float Rate);
	void StartReload();
};
