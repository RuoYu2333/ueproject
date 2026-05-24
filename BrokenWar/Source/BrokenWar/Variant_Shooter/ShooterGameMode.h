// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameMode.generated.h"

class UShooterUI;
class APlayerStart;

UENUM(BlueprintType)
enum class EShooterMatchStage : uint8
{
	ScavengeStage UMETA(DisplayName = "ScavengeStage"),
	DuelStage     UMETA(DisplayName = "DuelStage"),
	GameOverStage UMETA(DisplayName = "GameOverStage")
};

/**
 * Shooter GameMode
 * 前 180 秒：两个玩家分别在各自区域杀怪
 * 180 秒后：两个玩家传送到对战场
 */
UCLASS(abstract)
class BROKENWAR_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	/** Type of UI widget to spawn */
	UPROPERTY(EditAnywhere, Category = "Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

	/** Pointer to the UI widget */
	UPROPERTY()
	TObjectPtr<UShooterUI> ShooterUI;

	/** Map of scores by team ID */
	TMap<uint8, int32> TeamScores;

protected:
	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** 玩家加入服务器时调用 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 玩家退出时调用 */
	virtual void Logout(AController* Exiting) override;

	/** 控制玩家出生点 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

public:
	/** Increases the score for the given team */
	void IncrementTeamScore(uint8 TeamByte);

protected:
	/** 需要的玩家数量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter|MatchRules")
	int32 RequiredPlayerCount = 2;

	/** 杀怪阶段持续时间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter|MatchRules")
	int32 ScavengeTimeDuration = 30;

	/** 当前剩余时间 */
	UPROPERTY(BlueprintReadOnly, Category = "Shooter|MatchRules")
	int32 TimeRemaining = 30;

	/** 当前比赛阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "Shooter|MatchRules")
	EShooterMatchStage CurrentStage = EShooterMatchStage::ScavengeStage;

private:
	FTimerHandle MatchTimerHandle;

	bool bCountdownStarted = false;

	/** 记录每个玩家被分配到哪个区域，0 或 1 */
	TMap<AController*, int32> PlayerIndexMap;

private:
	void TryStartScavengeStage();
	void CountDownTimer();
	void EnterDuelStage();

	int32 GetOrAssignPlayerIndex(AController* Player);
	APlayerStart* FindTaggedPlayerStart(FName TargetTag) const;
	void TeleportPlayerToTaggedStart(APlayerController* PlayerController, FName TargetTag);

	/** 进入对战阶段后，清理杀怪阶段的 NPC 和刷怪器 */
	void ClearScavengeNPCsAndSpawners();
};