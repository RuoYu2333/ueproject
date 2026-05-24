#include "BrokenWarGameMode.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

ABrokenWarGameMode::ABrokenWarGameMode()
{
	ScavengeTimeDuration = 180; // 3分钟
	TimeRemaining = ScavengeTimeDuration;
	CurrentStage = EGameStage::ScavengeStage;
}

void ABrokenWarGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 仅在服务端启动全局定时器，每 1 秒执行一次 CountDownTimer()
	GetWorldTimerManager().SetTimer(GameTimerHandle, this, &ABrokenWarGameMode::CountDownTimer, 1.0f, true);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("GAME BEGINE！！"));
}

void ABrokenWarGameMode::CountDownTimer()
{
	if (TimeRemaining > 0)
	{
		TimeRemaining--;

		// 每一秒在屏幕上通知一下玩家（后续可以绑定到 UI 上显示）
		if (TimeRemaining % 10 == 0 || TimeRemaining <= 10)
		{
			FString TimeMsg = FString::Printf(TEXT("%d seconds remaind"), TimeRemaining);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TimeMsg);
		}
	}
	else
	{
		// 时间归零，清除定时器并触发大传送
		GetWorldTimerManager().ClearTimer(GameTimerHandle);
		TransitionToDuel();
	}
}

void ABrokenWarGameMode::TransitionToDuel()
{
	CurrentStage = EGameStage::DuelStage;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red, TEXT("Fight！"));

	int32 PlayerIdx = 0;
	// 遍历服务器上所有在线的玩家控制器 (PlayerController)
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn())
		{
			// 1. 临时禁用输入，防止传送瞬间发生位置抖动
			PC->GetPawn()->DisableInput(PC);

			// 2. 寻找我们在关卡里摆放好的对战出生点 (Tag 标记为 Duel_1, Duel_2)
			AActor* DuelSpawnPoint = FindDuelStartAnchor(PlayerIdx);
			if (DuelSpawnPoint)
			{
				// 3. 强行执行权威瞬移
				FVector SpawnLoc = DuelSpawnPoint->GetActorLocation();
				FRotator SpawnRot = DuelSpawnPoint->GetActorRotation();

				ACharacter* PlayerChar = Cast<ACharacter>(PC->GetPawn());
				if (PlayerChar)
				{
					// 使用最安全的 TeleportTo 规避碰撞卡死 Bug
					PlayerChar->TeleportTo(SpawnLoc, SpawnRot);
				}
			}

			// 4. 传送完成，恢复输入
			PC->GetPawn()->EnableInput(PC);
			PlayerIdx++;
		}
	}
}

AActor* ABrokenWarGameMode::FindDuelStartAnchor(int32 PlayerIndex)
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundActors);

	// 拼接我们在关卡里手动配好的出生点 Tag 名字 ("Duel_0", "Duel_1")
	FName TargetTag = FName(*FString::Printf(TEXT("Duel_%d"), PlayerIndex));

	for (AActor* Actor : FoundActors)
	{
		if (Actor && Actor->ActorHasTag(TargetTag))
		{
			return Actor;
		}
	}

	// 如果找不到特定的 Tag，就保底返回找到的第一个点
	return FoundActors.IsValidIndex(0) ? FoundActors[0] : nullptr;
}