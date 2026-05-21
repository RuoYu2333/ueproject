// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "ShooterBulletCounterUI.h"
#include "BrokenWar.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 手机摇杆跟具体的角色肉体无关，只需要在开局创建一次即可，放在这里非常安全
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				MobileControlsWidget->AddToPlayerScreen(0);
			}
			else
			{
				UE_LOG(LogBrokenWar, Error, TEXT("Could not spawn mobile controls widget."));
			}
		}

		// 【核心修改】：把你原本写在这里的 BulletCounterUI 创建代码全部删掉！
		// 绝对不能在这里创建子弹 UI。
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// OnPossess 只在服务器执行。
	// 绝对不要在这里碰 UI！只保留服务端关心的逻辑：监听玩家死亡（Pawn Destroyed）以便复活。
	if (InPawn)
	{
		InPawn->OnDestroyed.AddDynamic(this, &AShooterPlayerController::OnPawnDestroyed);
	}
}
// 此函数只在“拥有该角色的本地客户端”上运行
void AShooterPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	// 只允许本地控制器处理 UI
	if (!IsLocalController() || !InPawn)
	{
		return;
	}

	AShooterCharacter* ShooterChar = Cast<AShooterCharacter>(InPawn);
	if (!ShooterChar)
	{
		return;
	}
	OnPawnDamaged((ShooterChar->GetCurrentHP() / 100.0f));
	// 创建子弹 UI
	if (BulletCounterUIClass && !BulletCounterUI)
	{
		BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);
		if (BulletCounterUI)
		{
			BulletCounterUI->AddToViewport();
		}
	}

	// 先解绑，防止重复绑定
	ShooterChar->OnBulletCountUpdated.RemoveDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
	ShooterChar->OnDamaged.RemoveDynamic(this, &AShooterPlayerController::OnPawnDamaged);

	// 再绑定
	ShooterChar->OnBulletCountUpdated.AddDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
	ShooterChar->OnDamaged.AddDynamic(this, &AShooterPlayerController::OnPawnDamaged);

	// 刷新一次 UI
	if (BulletCounterUI)
	{
		if (AShooterWeapon* CurrentWeapon = ShooterChar->GetActiveWeapon())
		{
			BulletCounterUI->BP_UpdateBulletCounter(CurrentWeapon->GetMagazineSize(), CurrentWeapon->GetBulletCount());
		}
		else
		{
			BulletCounterUI->BP_UpdateBulletCounter(0, 0);
		}
	}
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !CharacterClass)
	{
		return;
	}
	// reset the bullet counter HUD
	

	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// select a random player start
		AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];

		// spawn a character at the player start
		const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

		if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
		{
			// possess the character
			Possess(RespawnedCharacter);
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	// update the UI
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
