#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BrokenWarGameMode.generated.h"

// 定义游戏所处的阶段枚举
UENUM(BlueprintType)
enum class EGameStage : uint8
{
	ScavengeStage    UMETA(DisplayName = "GAMEMODE:SEARCHING"),
	DuelStage        UMETA(DisplayName = "GAMEMODE:PK"),
	GameOverStage    UMETA(DisplayName = "GAMEMODE:GAMEOVER")
};

UCLASS()
class BROKENWAR_API ABrokenWarGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABrokenWarGameMode();

protected:
	virtual void BeginPlay() override;

	/** 全局搜刮时间（秒），默认 120 秒（2分钟） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameRules")
	int32 ScavengeTimeDuration;

	/** 当前剩余时间 */
	int32 TimeRemaining;

	/** 当前游戏所处的阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "GameRules")
	EGameStage CurrentStage;

	FTimerHandle GameTimerHandle;

	/** 每一秒执行一次的倒计时计数器 */
	void CountDownTimer();

	/** 核心逻辑：两分钟到，开始执行全场大传送 */
	void TransitionToDuel();

	/** 辅助函数：根据玩家编号寻找对战区的出生点 */
	AActor* FindDuelStartAnchor(int32 PlayerIndex);
};