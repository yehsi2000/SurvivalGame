// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapons/Weapon.h"

#include "SurvivalGame.h"

#include "Player/SurvivalPlayerController.h"
#include "Player/SurvivalCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InventoryComponent.h"
#include "Curves/CurveVector.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

#include "Net/UnrealNetwork.h"
#include "Items/EquippableItem.h"
#include "Items/AmmoItem.h"
#include "DrawDebugHelpers.h"

// Sets default values
AWeapon::AWeapon()
{
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->bReceivesDecals = false; //no blood on weapon
	WeaponMesh->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RootComponent = WeaponMesh;

	bLoopedMuzzleFX = false;
	bLoopedFireAnim = false;
	bPlayingFireAnim = false;
	bIsEquipped = false;
	bWantsToFire = false;
	bPendingReload = false;
	bPendingEquip = false;
	CurrentState = EWeaponState::Idle;
	AttachSocket1P = FName("GripPoint");
	AttachSocket3P = FName("GripPoint");
	MuzzleAttachPoint = FName("Muzzle");

	CurrentAmmoInClip = 0;
	BurstCounter = 0;
	LastFireTime = 0.0f;

	ADSTime = 0.5f;
	RecoilResetSpeed = 5.f;
	RecoilSpeed = 10.f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bNetUseOwnerRelevancy = true; //weapon's relevent as long as player's relevent
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeapon, PawnOwner);

	DOREPLIFETIME_CONDITION(AWeapon, CurrentAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWeapon, BurstCounter, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeapon, bPendingReload, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeapon, Item, COND_InitialOnly); //only once
	
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) {
		PawnOwner = Cast<ASurvivalCharacter>(GetOwner());
	}
	
}

void AWeapon::Destroyed()
{
	Super::Destroyed();
	StopSimulatingWeaponFire();
}

void AWeapon::UseClipAmmo()
{
	if (HasAuthority()) {
		--CurrentAmmoInClip;
	}
}

void AWeapon::ConsumeAmmo(const int32 Amount)
{
	if (HasAuthority() && PawnOwner) {
		if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory) {
			if (UItem* AmmoItem = Inventory->FindItemByClass(WeaponConfig.AmmoClass)) {
				Inventory->ConsumeItem(AmmoItem, Amount);
			}
		}
	}
}

void AWeapon::ReturnAmmoToInventory()
{
	if (HasAuthority() && PawnOwner && CurrentAmmoInClip>0) {
		if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory) {
			Inventory->TryAddItemFromClass(WeaponConfig.AmmoClass, CurrentAmmoInClip);
		}
	}
}

void AWeapon::OnEquip()
{
	AttachMeshToPawn();

	bPendingEquip = true;
	DetermineWeaponState();

	OnEquipFinished();

	if (PawnOwner && PawnOwner->IsLocallyControlled()) {
		PlayWeaponSound(EquipSound);
	}
}

void AWeapon::OnEquipFinished()
{
	AttachMeshToPawn();

	bIsEquipped = true;
	bPendingEquip = false;

	//determine the state so that reload checks will work
	DetermineWeaponState();

	if (PawnOwner) {
		if (PawnOwner->IsLocallyControlled() && CanReload()) {
			StartReload();
		}
	}
	
}

void AWeapon::OnUnEquip()
{
	bIsEquipped = false;
	StopFire();

	if (bPendingReload) {
		StopWeaponAnimation(ReloadAnim);
		bPendingReload = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
	}

	if (bPendingEquip) {
		StopWeaponAnimation(EquipAnim);
		bPendingEquip = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}

	ReturnAmmoToInventory();
	DetermineWeaponState();
}

bool AWeapon::IsEquipped() const
{
	return bIsEquipped;
}

bool AWeapon::IsAttachedToPawn() const
{
	return bIsEquipped || bPendingEquip;
}

void AWeapon::StartFire()
{
	if (!HasAuthority()) {
		ServerStartFire();
	}

	if (!bWantsToFire) {
		bWantsToFire = true;
		DetermineWeaponState();
	}
}

void AWeapon::StopFire()
{
	if (!HasAuthority()) {
		ServerStopFire();
	}

	if (bWantsToFire) {
		bWantsToFire = false;
		DetermineWeaponState();
	}
}

void AWeapon::StartReload(bool bFromReplication /*= false (true : server forced this reload)*/)
{
	if (!bFromReplication && !HasAuthority()) {
		ServerStartReload();
	}

	if (bFromReplication || CanReload()) {
		bPendingReload = true;
		DetermineWeaponState();
		float AnimDuration = PlayWeaponAnimation(ReloadAnim);
		if (AnimDuration <= 0.0f) {
			AnimDuration = 0.5f;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &AWeapon::StopReload, AnimDuration, false);
		if (HasAuthority()) {
			GetWorldTimerManager().SetTimer(TimerHandle_ReloadWeapon, this, &AWeapon::ReloadWeapon, FMath::Max(0.1f, AnimDuration - 0.1f), false);
		}

		if (PawnOwner && PawnOwner->IsLocallyControlled()) {
			PlayWeaponSound(ReloadSound);
		}
	}
}

void AWeapon::StopReload()
{
	if (CurrentState == EWeaponState::Reloading) {
		bPendingReload = false;
		DetermineWeaponState();
		StopWeaponAnimation(ReloadAnim);
	}
}

void AWeapon::ReloadWeapon()
{
	const int32 AmmoRefill = FMath::Min(WeaponConfig.AmmoPerClip - CurrentAmmoInClip, GetCurrentAmmo());

	if (AmmoRefill > 0) {
		CurrentAmmoInClip += AmmoRefill;
		ConsumeAmmo(AmmoRefill);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Didn't have enough ammo for a reload"));
	}
}

bool AWeapon::CanFire() const
{
	bool bCanFire = PawnOwner != nullptr;
	bool bStateOKToFire = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	//return ((bCanFire == true) && (bStateOKToFire == true) && (bPendingReload == false));
	return (bCanFire && bStateOKToFire && !bPendingReload);
}

bool AWeapon::CanReload() const
{
	bool bCanReload = PawnOwner != nullptr;
	bool bGotAmmo = (CurrentAmmoInClip < WeaponConfig.AmmoPerClip) && (GetCurrentAmmo() > 0);
	bool bStateOKToReload = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	return (bCanReload && bGotAmmo && bStateOKToReload);
}

EWeaponState AWeapon::GetCurrentState() const
{
	return CurrentState;
}

int32 AWeapon::GetCurrentAmmo() const
{
	if (PawnOwner) {
		if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory) {
			if (UItem* AmmoItem = Inventory->FindItemByClass(WeaponConfig.AmmoClass)) {
				return AmmoItem->GetQuantity();
			}
		}
	}

	return 0;
}

int32 AWeapon::GetCurrentAmmoInClip() const
{
	return CurrentAmmoInClip;
}

int32 AWeapon::GetAmmoPerClip() const
{
	return WeaponConfig.AmmoPerClip;
}

USkeletalMeshComponent* AWeapon::GetWeaponMesh() const
{
	return WeaponMesh;
}

ASurvivalCharacter* AWeapon::GetPawnOwner() const
{
	return PawnOwner;
}

void AWeapon::SetPawnOwner(ASurvivalCharacter* SurvivalCharacter)
{
	if (PawnOwner != SurvivalCharacter) {
		SetInstigator(SurvivalCharacter);
		PawnOwner = SurvivalCharacter;
		SetOwner(SurvivalCharacter);
	}
}

float AWeapon::GetEquipStartedTime() const
{
	return EquipStartedTime;
}

float AWeapon::GetEquipDuration() const
{
	return EquipDuration;
}

void AWeapon::ClientStartReload_Implementation()
{
	StartReload();
}


void AWeapon::ServerStartFire_Implementation()
{
	StartFire();
}

bool AWeapon::ServerStartFire_Validate()
{
	return true;
}

void AWeapon::ServerStopFire_Implementation()
{
	StopFire();
}

bool AWeapon::ServerStopFire_Validate()
{
	return true;
}

void AWeapon::ServerStartReload_Implementation()
{
	StartReload();
}

bool AWeapon::ServerStartReload_Validate()
{
	return true;
}

void AWeapon::ServerStopReload_Implementation()
{
	StopReload();
}

bool AWeapon::ServerStopReload_Validate()
{
	return true;
}

void AWeapon::OnRep_PawnOwner()
{

}

void AWeapon::OnRep_BurstCounter()
{
	if (BurstCounter > 0) {
		SimulateWeaponFire();
	}
	else {
		StopSimulatingWeaponFire();
	}
}

void AWeapon::OnRep_Reload()
{
	if (bPendingReload) {
		StartReload();
	}
	else {
		StopReload();
	}
}

void AWeapon::SimulateWeaponFire()
{
	//only showing gun particles and thing

	if (HasAuthority() && CurrentState != EWeaponState::Firing) {
		return;
	}
	if (MuzzleFX) {
		if (!bLoopedMuzzleFX || MuzzleFX == nullptr) {
			if ((PawnOwner != nullptr) && PawnOwner->IsLocallyControlled()) {
				if (PawnOwner->GetController() != nullptr) {
					WeaponMesh->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, WeaponMesh, MuzzleAttachPoint);
					MuzzlePSC->bOwnerNoSee = false;
					MuzzlePSC->bOnlyOwnerSee = true;
				}
			}
			else
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, WeaponMesh, MuzzleAttachPoint);
			}
		}
	}

	if (bLoopedFireAnim && bPlayingFireAnim) {
		PlayWeaponAnimation(FireAnim);
		bPlayingFireAnim = true;
	}

	if (bLoopedFireSound) {
		if (FireAC == nullptr) FireAC = PlayWeaponSound(FireLoopSound);
	}
	else
	{
		PlayWeaponSound(FireSound);
	}

	ASurvivalPlayerController* PC = (PawnOwner != nullptr) ? Cast<ASurvivalPlayerController>(PawnOwner->GetController()) : nullptr;
	if (PC != nullptr && PC->IsLocalPlayerController()) {
		if (FireCameraShake != nullptr) {
			PC->ClientPlayCameraShake(FireCameraShake, 1);
		}
		if (FireForceFeedback != nullptr) {
			FForceFeedbackParameters FFParams;
			FFParams.Tag = "Weapon";
			PC->ClientPlayForceFeedback(FireForceFeedback, FFParams);
		}
	}
}

void AWeapon::StopSimulatingWeaponFire()
{
	if (bLoopedMuzzleFX) {
		if (MuzzlePSC != nullptr) {
			MuzzlePSC->DeactivateSystem();
			MuzzlePSC = nullptr;
		}
		if (MuzzlePSCSecondary != nullptr) {
			MuzzlePSCSecondary->DeactivateSystem();
			MuzzlePSCSecondary = nullptr;
		}
	}

	if (bLoopedFireAnim && bPlayingFireAnim) {
		StopWeaponAnimation(FireAimingAnim);
		StopWeaponAnimation(FireAnim);
		bPlayingFireAnim = false;
	}

	if (FireAC) {
		FireAC->FadeOut(0.1f, 0.0f);
		FireAC = nullptr;
		PlayWeaponSound(FireFinishSound);
	}
}

void AWeapon::HandleHit(const FHitResult& Hit, class ASurvivalCharacter* HitPlayer /*= nullptr*/)
{
	if (Hit.GetActor()) {
		UE_LOG(LogTemp, Warning, TEXT("Hit actor %s"), *Hit.GetActor()->GetName());
	}

	ServerHandleHit(Hit, HitPlayer);

	if (HitPlayer && PawnOwner) {
		if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(PawnOwner->GetController())) {
			PC->OnHitPlayer();
		}
	}
}

void AWeapon::ServerHandleHit_Implementation(const FHitResult& Hit, class ASurvivalCharacter* HitPlayer /*= nullptr*/)
{
	if (PawnOwner) {
		float DamageMultiplier = 1.f;

		for (auto& BoneDamageModifier : HitScanConfig.BoneDamageModifiers) {
			if (Hit.BoneName == BoneDamageModifier.Key) {
				DamageMultiplier = BoneDamageModifier.Value;
				break;
			}
		}

		if (HitPlayer) {
			UGameplayStatics::ApplyPointDamage(HitPlayer, HitScanConfig.Damage * DamageMultiplier, (Hit.TraceStart - Hit.TraceEnd).GetSafeNormal(), Hit, PawnOwner->GetController(), this, HitScanConfig.DamageType);
		}
	}
}

bool AWeapon::ServerHandleHit_Validate(const FHitResult& Hit, class ASurvivalCharacter* HitPlayer /*= nullptr*/)
{
	return true;
}

void AWeapon::FireShot()
{
	if (PawnOwner) {
		if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(PawnOwner->GetController())) {
			if (RecoilCurve) {
				const FVector2D RecoilAmount(RecoilCurve->GetVectorValue(FMath::RandRange(0.f, 1.f)).X, RecoilCurve->GetVectorValue(FMath::RandRange(0.f, 1.f)).Y);
				PC->ApplyRecoil(RecoilAmount, RecoilSpeed, RecoilResetSpeed, FireCameraShake);
			}

			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);

			FHitResult Hit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(PawnOwner);

			FVector FireDir = CamRot.Vector();// PawnOwner->IsAiming() ? CamRot.Vector() : FMath::VRandCone(CamRot.Vector(), FMath::DegreesToRadians(PawnOwner->IsAiming() ? 0.f : 5.f));
			FVector TraceStart = CamLoc;
			FVector TraceEnd = (FireDir * HitScanConfig.Distance) + CamLoc;

			if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, COLLISION_WEAPON, QueryParams)){
				ASurvivalCharacter* HitChar = Cast<ASurvivalCharacter>(Hit.GetActor());

				HandleHit(Hit, HitChar);

				FColor PointColor = FColor::Red;
				DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 5.f, PointColor, false, 30.f);
			}
		}
	}
}

void AWeapon::ServerHandleFiring_Implementation()
{
	const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());
	HandleFiring();

	if (bShouldUpdateAmmo) {
		UseClipAmmo();

		BurstCounter++;
	}
}

bool AWeapon::ServerHandleFiring_Validate()
{
	return true;
}

void AWeapon::HandleReFiring()
{
	UWorld* MyWorld = GetWorld();

	float SlackTimeThisFrame = FMath::Max(0.0f, (MyWorld->TimeSeconds - LastFireTime) - WeaponConfig.TimeBetweenShots);

	if (bAllowAutomaticWeaponCatchup) {
		TimerIntervalAdjustment -= SlackTimeThisFrame;
	}

	HandleFiring();
}

void AWeapon::HandleFiring()
{
	if ((CurrentAmmoInClip > 0) && CanFire()) {
		if (GetNetMode() != NM_DedicatedServer) {
			SimulateWeaponFire();
		}

		if (PawnOwner && PawnOwner->IsLocallyControlled()) {
			FireShot();
			UseClipAmmo();

			//update firing FX on remote clients if function was called on server
			BurstCounter++;
		}
	}
	else if (CanReload()) {
		StartReload();
	}
	else if (PawnOwner && PawnOwner->IsLocallyControlled()) {
		if (GetCurrentAmmo() == 0 && !bRefiring) {
			PlayWeaponSound(OutOfAmmoSound);
			ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(PawnOwner->GetController());
		}

		if (BurstCounter > 0) //stop weapon fire fx, but stay in firing state
			OnBurstFinished();
	}

	if (PawnOwner && PawnOwner->IsLocallyControlled()) {
		if (!HasAuthority()) {
			ServerHandleFiring();
		}

		if (CurrentAmmoInClip <= 0 && CanReload()) {
			StartReload();
		}

		bRefiring = (CurrentState == EWeaponState::Firing && WeaponConfig.TimeBetweenShots > 0.f);
		if (bRefiring) {
			GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleReFiring, FMath::Max<float>(WeaponConfig.TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
			TimerIntervalAdjustment = 0.f;
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AWeapon::OnBurstStarted()
{
	// start firing, can be delayed to satisfy TimeBetweenShots
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0 && WeaponConfig.TimeBetweenShots > 0.0f &&	LastFireTime + WeaponConfig.TimeBetweenShots > GameTime)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleFiring, LastFireTime + WeaponConfig.TimeBetweenShots - GameTime, false);
	}
	else
	{
		HandleFiring();
	}
}

void AWeapon::OnBurstFinished()
{
	// stop firing FX on remote clients
	BurstCounter = 0;

	// stop firing FX locally, unless it's a dedicated server
	if (GetNetMode() != NM_DedicatedServer)
	{
		StopSimulatingWeaponFire();
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
	bRefiring = false;

	// reset firing interval adjustment
	TimerIntervalAdjustment = 0.0f;
}


void AWeapon::SetWeaponState(EWeaponState NewState)
{
	const EWeaponState PrevState = CurrentState;

	if (PrevState == EWeaponState::Firing && NewState != EWeaponState::Firing)
	{
		OnBurstFinished();
	}

	CurrentState = NewState;

	if (PrevState != EWeaponState::Firing && NewState == EWeaponState::Firing)
	{
		OnBurstStarted();
	}
}

void AWeapon::DetermineWeaponState()
{
	EWeaponState NewState = EWeaponState::Idle;

	if (bIsEquipped)
	{
		if (bPendingReload)
		{
			if (CanReload() == false)
			{
				NewState = CurrentState;
			}
			else
			{
				NewState = EWeaponState::Reloading;
			}
		}
		else if (!bPendingReload && bWantsToFire && CanFire())
		{
			NewState = EWeaponState::Firing;
		}
	}
	else if (bPendingEquip)
	{
		NewState = EWeaponState::Equipping;
	}

	SetWeaponState(NewState);
}

void AWeapon::AttachMeshToPawn()
{
	if (PawnOwner)
	{
		if (const USkeletalMeshComponent* PawnMesh = PawnOwner->GetMesh()) {
			const FName AttachSocket = PawnOwner->IsLocallyControlled() ? AttachSocket1P : AttachSocket3P;
			AttachToComponent(PawnOwner->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
		}
		
	}
}

UAudioComponent* AWeapon::PlayWeaponSound(USoundCue* Sound)
{
	UAudioComponent* AC = nullptr;
	if (Sound && PawnOwner)
	{
		AC = UGameplayStatics::SpawnSoundAttached(Sound, PawnOwner->GetRootComponent());
	}

	return AC;
}

float AWeapon::PlayWeaponAnimation(const FWeaponAnim& Animation)
{
	float Duration = 0.0f;
	if (PawnOwner)
	{
		UAnimMontage* UseAnim = PawnOwner->IsLocallyControlled() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			Duration = PawnOwner->PlayAnimMontage(UseAnim);
		}
	}

	return Duration;
}

void AWeapon::StopWeaponAnimation(const FWeaponAnim& Animation)
{
	if (PawnOwner)
	{
		UAnimMontage* UseAnim = PawnOwner->IsLocallyControlled() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			PawnOwner->StopAnimMontage(UseAnim);
		}
	}
}

FVector AWeapon::GetCameraAim() const
{
	ASurvivalPlayerController* const PlayerController = GetInstigator() ? Cast<ASurvivalPlayerController>(GetInstigator()->Controller) : NULL;
	FVector FinalAim = FVector::ZeroVector;

	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();
	}

	return FinalAim;
}

FHitResult AWeapon::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const
{
	// Perform trace to retrieve hit info
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, COLLISION_WEAPON, TraceParams);

	return Hit;
}
