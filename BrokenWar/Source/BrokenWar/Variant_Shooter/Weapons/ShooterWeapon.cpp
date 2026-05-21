// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/UnrealNetwork.h"
#include "ShooterWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

AShooterWeapon::AShooterWeapon()
{
	//For CS
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	// subscribe to the owner's destroyed delegate
	GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
	PawnOwner = Cast<APawn>(GetOwner());

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::ActivateWeapon()
{
	// unhide this weapon
	SetActorHiddenInGame(false);

	// notify the owner
	WeaponOwner->OnWeaponActivated(this);
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();

	// hide the weapon
	SetActorHiddenInGame(true);

	// notify the owner
	WeaponOwner->OnWeaponDeactivated(this);
}

void AShooterWeapon::StartFiring()
{
	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	// this may be under the refire rate if the weapon shoots slow enough and the player is spamming the trigger
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot > RefireRate)
	{
		// fire the weapon right away
		Fire();

	} else {

		// if we're full auto, schedule the next shot
		if (bFullAuto)
		{
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, TimeSinceLastShot, false);
		}

	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::Fire()
{
	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring)
	{
		return;
	}
	
	// fire a projectile at the target
	FireProjectile(WeaponOwner->GetWeaponTargetLocation());

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	WeaponOwner->OnSemiWeaponRefire();
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	// 1. 本地预测 (Local Prediction)
	// 立刻播放第一人称开火动画和后坐力，保证本地玩家按下左键时拥有 0 延迟的极致手感
	if (WeaponOwner)
	{
		WeaponOwner->PlayFiringMontage(FiringMontage);
		WeaponOwner->AddWeaponRecoil(FiringRecoil);
	}

	// 2. 权限分发：呼叫服务器去执行真实的物理生成和弹药扣除
	if (HasAuthority())
	{
		// 如果开火的人本身就是服务器（或者在单机/监听服本地），直接执行真实开火
		Server_Fire_Implementation(TargetLocation);
	}
	else
	{
		// 如果是客户端，发送 RPC 请求给服务器，并把当前瞄准的目标位置 TargetLocation 传过去
		Server_Fire(TargetLocation);
	}
}
//验证rpc合法性

bool AShooterWeapon::Server_Fire_Validate(FVector TargetLocation)
{
	return true;
}
void AShooterWeapon::Server_Fire_Implementation(FVector TargetLocation)
{
	// 权威拦截：只有服务器确认你的子弹数大于 0，你才被允许真正开火
	if (CurrentBullets > 0)
	{
		// 1. 扣除真实弹药（由于 CurrentBullets 加上了 Replicated，全场数据会自动对齐同步）
		--CurrentBullets;

		// 如果弹匣打空，自动填满（保留你原本的自动装填机制）
		if (CurrentBullets <= 0)
		{
			CurrentBullets = MagazineSize;
		}

		// 更新服务端的 HUD 状态
		if (WeaponOwner)
		{
			WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
		}

		// 2. 服务端权威生成子弹：计算抛射体变换并 Spawn 拥有真实物理和伤害判定的子弹实体
		FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = PawnOwner;

		// 只有服务器生成的 Projectile 才是合法的，客户端只能通过网络复制看到它
		GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

		// 3. 呼叫全场广播，让除了开火者之外的其他所有联机玩家也能看到你开火
		Multicast_FireEffects();
	}
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// find the muzzle location
	const FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

	// calculate the spawn location ahead of the muzzle
	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	// find the aim rotation vector while applying some variance to the target 
	const FRotator AimRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance));

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}
void AShooterWeapon::Multicast_FireEffects_Implementation()
{
	// 核心过滤条件：只有“不是本地控制的玩家”才需要在这里播放。
	// 为什么？因为开火者自己已经在第一步“本地预测”阶段提前播过动画和后坐力了，不能重复播放两次。
	if (PawnOwner && !PawnOwner->IsLocallyControlled() && WeaponOwner)
	{
		// 让其他玩家在他们的屏幕上看到这个角色播放开火 Montage
		WeaponOwner->PlayFiringMontage(FiringMontage);

		// 同样在其他玩家的画面里同步模拟这个人的枪口上抬后坐力（或者用于模拟第三人称的表现）
		WeaponOwner->AddWeaponRecoil(FiringRecoil);
	}
}
void AShooterWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 注册弹药同步
	DOREPLIFETIME(AShooterWeapon, CurrentBullets);
}
