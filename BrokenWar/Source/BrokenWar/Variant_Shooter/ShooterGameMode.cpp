// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/ShooterGameMode.h"

#include "ShooterUI.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Kismet/GameplayStatics.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Variant_Shooter/AI/ShooterNPCSpawner.h"

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	TimeRemaining = ScavengeTimeDuration;
	CurrentStage = EShooterMatchStage::ScavengeStage;

	// 保留原来的 Shooter UI 创建逻辑
	APlayerController* FirstPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (FirstPC && ShooterUIClass)
	{
		ShooterUI = CreateWidget<UShooterUI>(FirstPC, ShooterUIClass);

		if (ShooterUI)
		{
			ShooterUI->AddToViewport(0);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Cyan,
			TEXT("ShooterGameMode: Waiting for two players...")
		);
	}
}

void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		GetOrAssignPlayerIndex(NewPlayer);
	}

	TryStartScavengeStage();
}

void AShooterGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		PlayerIndexMap.Remove(Exiting);
	}

	Super::Logout(Exiting);
}

AActor* AShooterGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const int32 PlayerIndex = GetOrAssignPlayerIndex(Player);

	FName TargetTag;

	if (CurrentStage == EShooterMatchStage::ScavengeStage)
	{
		TargetTag = FName(*FString::Printf(TEXT("Scavenge_%d"), PlayerIndex));
	}
	else if (CurrentStage == EShooterMatchStage::DuelStage)
	{
		TargetTag = FName(*FString::Printf(TEXT("Duel_%d"), PlayerIndex));
	}
	else
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	if (APlayerStart* TaggedStart = FindTaggedPlayerStart(TargetTag))
	{
		return TaggedStart;
	}

	UE_LOG(LogTemp, Warning, TEXT("Can not find PlayerStart with tag: %s"), *TargetTag.ToString());

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AShooterGameMode::TryStartScavengeStage()
{
	if (bCountdownStarted)
	{
		return;
	}

	if (GetNumPlayers() < RequiredPlayerCount)
	{
		if (GEngine)
		{
			const FString Msg = FString::Printf(
				TEXT("Waiting players: %d / %d"),
				GetNumPlayers(),
				RequiredPlayerCount
			);

			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Msg);
		}

		return;
	}

	bCountdownStarted = true;
	TimeRemaining = ScavengeTimeDuration;
	CurrentStage = EShooterMatchStage::ScavengeStage;

	GetWorldTimerManager().SetTimer(
		MatchTimerHandle,
		this,
		&AShooterGameMode::CountDownTimer,
		1.0f,
		true
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Green,
			TEXT("Scavenge stage started. Time: 180 seconds.")
		);
	}
}

void AShooterGameMode::CountDownTimer()
{
	if (TimeRemaining > 0)
	{
		TimeRemaining--;

		if (TimeRemaining % 10 == 0 || TimeRemaining <= 10)
		{
			const FString Msg = FString::Printf(
				TEXT("Scavenge time remaining: %d"),
				TimeRemaining
			);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, Msg);
			}
		}

		return;
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);

	EnterDuelStage();
}

void AShooterGameMode::EnterDuelStage()
{
	if (CurrentStage == EShooterMatchStage::DuelStage)
	{
		return;
	}

	CurrentStage = EShooterMatchStage::DuelStage;

	ClearScavengeNPCsAndSpawners();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.0f,
			FColor::Red,
			TEXT("Duel stage started!")
		);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();

		if (!PC)
		{
			continue;
		}

		const int32 PlayerIndex = GetOrAssignPlayerIndex(PC);

		const FName DuelStartTag = FName(*FString::Printf(TEXT("Duel_%d"), PlayerIndex));

		TeleportPlayerToTaggedStart(PC, DuelStartTag);
	}
}

int32 AShooterGameMode::GetOrAssignPlayerIndex(AController* Player)
{
	if (!Player)
	{
		return 0;
	}

	if (int32* ExistingIndex = PlayerIndexMap.Find(Player))
	{
		return *ExistingIndex;
	}

	TSet<int32> UsedIndices;

	for (const TPair<AController*, int32>& Pair : PlayerIndexMap)
	{
		UsedIndices.Add(Pair.Value);
	}

	int32 NewIndex = 0;

	for (int32 Index = 0; Index < RequiredPlayerCount; ++Index)
	{
		if (!UsedIndices.Contains(Index))
		{
			NewIndex = Index;
			break;
		}
	}

	PlayerIndexMap.Add(Player, NewIndex);

	return NewIndex;
}

APlayerStart* AShooterGameMode::FindTaggedPlayerStart(FName TargetTag) const
{
	TArray<AActor*> PlayerStartActors;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		APlayerStart::StaticClass(),
		PlayerStartActors
	);

	for (AActor* Actor : PlayerStartActors)
	{
		APlayerStart* PlayerStart = Cast<APlayerStart>(Actor);

		if (!PlayerStart)
		{
			continue;
		}

		// 支持 Actor Tags
		if (PlayerStart->ActorHasTag(TargetTag))
		{
			return PlayerStart;
		}

		// 支持 PlayerStart 自带的 PlayerStartTag
		if (PlayerStart->PlayerStartTag == TargetTag)
		{
			return PlayerStart;
		}
	}

	return nullptr;
}

void AShooterGameMode::TeleportPlayerToTaggedStart(APlayerController* PlayerController, FName TargetTag)
{
	if (!PlayerController)
	{
		return;
	}

	APlayerStart* TargetStart = FindTaggedPlayerStart(TargetTag);

	if (!TargetStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can not find duel PlayerStart: %s"), *TargetTag.ToString());
		return;
	}

	APawn* Pawn = PlayerController->GetPawn();

	if (!Pawn)
	{
		RestartPlayerAtPlayerStart(PlayerController, TargetStart);
		return;
	}

	const FVector TargetLocation = TargetStart->GetActorLocation();
	const FRotator TargetRotation = TargetStart->GetActorRotation();

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}

		Character->TeleportTo(TargetLocation, TargetRotation, false, true);
	}
	else
	{
		Pawn->SetActorLocationAndRotation(
			TargetLocation,
			TargetRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	PlayerController->SetControlRotation(TargetRotation);
}

void AShooterGameMode::ClearScavengeNPCsAndSpawners()
{
	static const FName ScavengeSpawnerTag = FName("ScavengeSpawner");
	static const FName ScavengeNPCTag = FName("ScavengeNPC");

	TArray<AActor*> SpawnerActors;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AShooterNPCSpawner::StaticClass(),
		SpawnerActors
	);

	for (AActor* SpawnerActor : SpawnerActors)
	{
		AShooterNPCSpawner* Spawner = Cast<AShooterNPCSpawner>(SpawnerActor);

		if (!Spawner)
		{
			continue;
		}

		if (Spawner->ActorHasTag(ScavengeSpawnerTag))
		{
			Spawner->StopSpawning(true);
			Spawner->Destroy();
		}
	}

	TArray<AActor*> NPCActors;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AShooterNPC::StaticClass(),
		NPCActors
	);

	for (AActor* NPCActor : NPCActors)
	{
		if (!NPCActor)
		{
			continue;
		}

		if (NPCActor->ActorHasTag(ScavengeNPCTag))
		{
			NPCActor->Destroy();
		}
	}
}
void AShooterGameMode::IncrementTeamScore(uint8 TeamByte)
{
	// retrieve the team score if any
	int32 Score = 0;

	if (int32* FoundScore = TeamScores.Find(TeamByte))
	{
		Score = *FoundScore;
	}

	// increment the score for the given team
	++Score;

	TeamScores.Add(TeamByte, Score);

	// update the UI
	if (ShooterUI)
	{
		ShooterUI->BP_UpdateScore(TeamByte, Score);
	}
}