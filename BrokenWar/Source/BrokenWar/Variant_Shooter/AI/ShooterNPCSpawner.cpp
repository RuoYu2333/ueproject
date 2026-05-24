// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/AI/ShooterNPCSpawner.h"

#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "ShooterNPC.h"

// Sets default values
AShooterNPCSpawner::AShooterNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

void AShooterNPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	bStopSpawning = false;
	CurrentSpawnedNPC = nullptr;

	// ensure we don't spawn NPCs if our initial spawn count is zero
	if (SpawnCount > 0)
	{
		// schedule the first NPC spawn
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimer,
			this,
			&AShooterNPCSpawner::SpawnNPC,
			InitialSpawnDelay,
			false
		);
	}
}

void AShooterNPCSpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the spawn timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	}

	CurrentSpawnedNPC = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AShooterNPCSpawner::SpawnNPC()
{
	// 如果已经停止刷怪，直接返回
	if (bStopSpawning)
	{
		return;
	}

	// ensure the NPC class is valid
	if (!IsValid(NPCClass))
	{
		return;
	}

	// spawn the NPC at the reference capsule's transform
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AShooterNPC* NewNPC = GetWorld()->SpawnActor<AShooterNPC>(
		NPCClass,
		SpawnCapsule->GetComponentTransform(),
		SpawnParams
	);

	// was the NPC successfully created?
	if (NewNPC)
	{
		CurrentSpawnedNPC = NewNPC;

		// 如果这个刷怪器是杀怪阶段刷怪器，那么给它生成的 NPC 也加标记
		// 这样 180 秒后 GameMode 可以准确清理杀怪阶段的 NPC
		if (ActorHasTag(FName("ScavengeSpawner")))
		{
			NewNPC->Tags.Add(FName("ScavengeNPC"));
		}

		// subscribe to the death delegate
		NewNPC->OnPawnDeath.AddDynamic(this, &AShooterNPCSpawner::OnNPCDied);
	}
}

void AShooterNPCSpawner::OnNPCDied()
{
	CurrentSpawnedNPC = nullptr;

	// 如果已经停止刷怪，不再继续生成
	if (bStopSpawning)
	{
		return;
	}

	// decrease the spawn counter
	--SpawnCount;

	// is this the last NPC we should spawn?
	if (SpawnCount <= 0)
	{
		return;
	}

	// schedule the next NPC spawn
	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimer,
		this,
		&AShooterNPCSpawner::SpawnNPC,
		RespawnDelay,
		false
	);
}

void AShooterNPCSpawner::StopSpawning(bool bDestroyCurrentNPC)
{
	bStopSpawning = true;

	// 停止还没执行的下一次刷怪计时器
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	}

	// 是否销毁当前已经刷出来的 NPC
	if (bDestroyCurrentNPC && IsValid(CurrentSpawnedNPC))
	{
		CurrentSpawnedNPC->Destroy();
		CurrentSpawnedNPC = nullptr;
	}
}