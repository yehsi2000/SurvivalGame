// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UAnimMontage;
class ASurvivalCharacter;
class UAudioComponent;
class UParticleSystemComponent;
class UCameraShakeBase;
class UForceFeedbackEffect;
class USoundCue;

UENUM(BlueprintType)
enum class EWeaponState : uint8 {
	Idle,
	Firing,
	Reloading,
	Equipping
};

USTRUCT(BlueprintType)
struct FWeaponData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	int32 AmmoPerClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	TSubclassOf<class UAmmoItem> AmmoClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponStat")
	float TimeBetweenShots;

	FWeaponData() {
		AmmoPerClip = 20;
		TimeBetweenShots = 0.2f;
	}
};

USTRUCT()
struct FWeaponAnim {
	GENERATED_USTRUCT_BODY()

	/** fov animation */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* Pawn1P;

	/** third person view animation, might not need*/
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* Pawn3P;
};

USTRUCT(BlueprintType)
struct FHitScanConfiguration {
	GENERATED_BODY()

	FHitScanConfiguration() {
		Distance = 10000.f;
		Damage = 25.f;
		Radius = 0.f;
		DamageType = UDamageType::StaticClass();
	}

	/** Map of bone-damage multiplier. If bone is child of given bone, it will use this damage amount
	ex) Head,2 means double damage on head*/
	UPROPERTY(EditDefaultsOnly, Category = "Trace Info")
	TMap<FName, float> BoneDamageModifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly ,Category = "Trace Info")
	float Distance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly ,Category = "Trace Info")
	float Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly ,Category = "Trace Info")
	float Radius;

	UPROPERTY(EditDefaultsOnly, Category = "WeaponStat")
	TSubclassOf<UDamageType> DamageType;

	
};

UCLASS()
class SURVIVALGAME_API AWeapon : public AActor
{
	GENERATED_BODY()

	friend class ASurvivalCharacter;
	
public:	
	// Sets default values for this actor's properties
	AWeapon();

public:
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void BeginPlay() override;
	void Destroyed() override;

protected:
	/** consume a bullet */
	void UseClipAmmo();

	/** consume ammo from inventory for reloading */
	void ConsumeAmmo(const int32 Amount);

	/** [Server] return leftover ammo to the inventory when unequipping weapon*/
	void ReturnAmmoToInventory();

	virtual void OnEquip();

	virtual void OnEquipFinished();

	virtual void OnUnEquip();

	bool IsEquipped() const;

	/** check is mesh is already attached */
	bool IsAttachedToPawn() const;

	////////////////////////////////////////////////
	// INPUT
	////////////////////////////////////////////////

	/** [local+server] start weapon fire */
	virtual void StartFire();

	/** [local+server] stop weapon fire */
	virtual void StopFire();

	/** [all] start weapon reload */
	virtual void StartReload(bool bFromReplication = false);

	/** [local+server] interrupt weapon reload*/
	virtual void StopReload();

	/** [server] performs actual reload */
	virtual void ReloadWeapon();

	/** trigger reload from server */
	UFUNCTION(Client, Reliable)
	void ClientStartReload();

	bool CanFire() const;
	bool CanReload() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	EWeaponState GetCurrentState() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	int32 GetCurrentAmmo() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	int32 GetCurrentAmmoInClip() const;

	/** get clip size */
	int32 GetAmmoPerClip() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintCallable, Category="Weapon")
	ASurvivalCharacter* GetPawnOwner() const;

	/** set the weapon's owning pawn */
	void SetPawnOwner(ASurvivalCharacter* SurvivalCharacter);

	/** gets last time when this weapon was switched to */
	float GetEquipStartedTime() const;

	/** gets the duration of equipping weapon */
	float GetEquipDuration() const;

protected:
	/** item class for inventory */
	UPROPERTY(Replicated, BlueprintReadOnly, Transient)
	class UWeaponItem* Item;

	/** character holding the gun */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_PawnOwner)
	class ASurvivalCharacter* PawnOwner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Config")
	FWeaponData WeaponConfig;

	/** line trace data, will be used if projectile class = null */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Config")
	FHitScanConfiguration HitScanConfig;

public:

	UPROPERTY(EditAnywhere, Category="Components")
	USkeletalMeshComponent* WeaponMesh;

protected:
	//might not use this
	/** Adjustment to handle frame rate affecting actual timer interval */
	UPROPERTY(Transient)
	float TimerIntervalAdjustment;

	//might not use this 
	/** Whether to allow automatic weapons to catch up with shorter refire cycles */
	UPROPERTY(Config)
	bool bAllowAutomaticWeaponCatchup = true;

	/** firing audio (bLoopedFireSound set) */
	UPROPERTY(Transient)
	UAudioComponent* FireAC;

	/** name of bone/socket for muzzle in weapon mesh */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName MuzzleAttachPoint;

	/** name of socket to attach to the character on */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName AttachSocket1P;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName AttachSocket3P;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* MuzzleFX;

	/** spawned component for muzzle fx */
	UPROPERTY(Transient)
	UParticleSystemComponent* MuzzlePSC;

	/** spawned component for second muzzle fx (needed for split screen) */
	UPROPERTY(Transient)
	UParticleSystemComponent* MuzzlePSCSecondary;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	/** the time takes to aim down sights, in seconds */
	UPROPERTY(EditDefaultsOnly, Category="Weapon")
	float ADSTime;

	/** The amount of recoil to apply.
	We choose random point from 0-1 on the curve and use it to drive recoil
	Designers get lots of control over the recoil pattern*/
	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	class UCurveVector* RecoilCurve;

	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilSpeed;

	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilResetSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UForceFeedbackEffect* FireForceFeedback;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireFinishSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* OutOfAmmoSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* ReloadSound;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim ReloadAnim;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* EquipSound;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim EquipAnim;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim FireAnim;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim FireAimingAnim;

	/** is muzzle fx looped */
	UPROPERTY(EditDefaultsOnly, Category= Effects)
	uint32 bLoopedMuzzleFX : 1;

	/** is fire anim looped */
	UPROPERTY(EditDefaultsOnly, Category= Sound)
	uint32 bLoopedFireSound : 1;

	/** is fire anim looped */
	UPROPERTY(EditDefaultsOnly, Category= Animation)
	uint32 bLoopedFireAnim : 1;

	uint32 bPlayingFireAnim : 1;

	uint32 bIsEquipped : 1;

	uint32 bWantsToFire : 1;

	/** is reload animation playing */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_Reload)
	uint32 bPendingReload : 1;

	/** is equip animation playing */
	uint32 bPendingEquip : 1;

	/** weapon is refiring */
	uint32 bRefiring;

	/** time of last successful weapon fire */
	float LastFireTime;

	/** last time when this weapon was switched to */
	float EquipStartedTime;

	/** how much time weapon needs to be equipped */
	float EquipDuration;

	EWeaponState CurrentState;

	/** current ammo - inside clip */
	UPROPERTY(Transient, Replicated)
	int32 CurrentAmmoInClip;

	/** burst counter , used for replicating fire events to remote clients */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
	int32 BurstCounter;

	/** Handle for efficient management of OnEquipFinished timer */
	FTimerHandle TimerHandle_OnEquipFinished;

	/** Handle for efficient management of StopReload timer */
	FTimerHandle TimerHandle_StopReload;

	/** Handle for efficient management of ReloadWeapon timer */
	FTimerHandle TimerHandle_ReloadWeapon;

	/** Handle for efficient management of HandleFiring timer */
	FTimerHandle TimerHandle_HandleFiring;

	////////////////////////////////////////////////
	// SERVER-SIDE INPUT
	////////////////////////////////////////////////

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStartFire();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStopFire();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStartReload();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStopReload();

	////////////////////////////////////////////////
	// REPLICATION & EFFECTS
	////////////////////////////////////////////////
	UFUNCTION()
	void OnRep_PawnOwner();

	UFUNCTION()
	void OnRep_BurstCounter();

	UFUNCTION()
	void OnRep_Reload();

	/** Called in network play to do the cosmetic fx for firing */
	virtual void SimulateWeaponFire();

	/** Called in network play to stop cosmetic fx(e.g. for a looping shot) */
	virtual void StopSimulatingWeaponFire();

	////////////////////////////////////////////////
	// REPLICATION & EFFECTS
	////////////////////////////////////////////////

	/** handle hit locally before asking server to process it */
	void HandleHit(const FHitResult& Hit, class ASurvivalCharacter* HitPlayer = nullptr);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerHandleHit(const FHitResult& Hit, class ASurvivalCharacter* HitPlayer = nullptr);

	/** [local] weapon specific fire implementation */
	virtual void FireShot();

	/** [server] fire & update ammo */
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerHandleFiring();

	/** [local+server] handle weapon refire, compensating for slack time if the timer can't sample fast enough */
	void HandleReFiring();

	/** [local+server] handle weapon fire */
	void HandleFiring();

	/** [local + server] firing started */
	virtual void OnBurstStarted();

	/** [local + server] firing finished */
	virtual void OnBurstFinished();

	/** update weapon state */
	void SetWeaponState(EWeaponState NewState);

	/** determine current weapon state */
	void DetermineWeaponState();

	/** attaches wapon mesh to pawn's mesh (detachment is automatic)*/
	void AttachMeshToPawn();

	////////////////////////////////////////////////
	// WEAPON USAGE HELPERS
	////////////////////////////////////////////////

	UAudioComponent* PlayWeaponSound(USoundCue* Sound);

	float PlayWeaponAnimation(const FWeaponAnim& Animation);

	void StopWeaponAnimation(const FWeaponAnim& Animation);

	FVector GetCameraAim() const;

	/** find hit */
	FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const;
};


