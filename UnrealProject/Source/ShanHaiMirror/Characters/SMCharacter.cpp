#include "SMCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include "Framework/SHMAttributeComponent.h"
#include "Framework/SHMWeaponComponent.h"
#include "Framework/SHMAbilityComponent.h"
#include "Framework/SHMBuildComponent.h"
#include "Framework/SHMEventBus.h"
#include "Framework/SHMGameplayTags.h"
#include "Combat/SHMProjectile.h"
#include "Director/SHMDirectorCore.h"

ASMCharacter::ASMCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 俯视角 twin-stick：朝向手动控制（面朝鼠标），不跟随移动方向
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// ===== 组件 =====
	AttributeComp = CreateDefaultSubobject<USHMAttributeComponent>(TEXT("AttributeComp"));
	WeaponComp    = CreateDefaultSubobject<USHMWeaponComponent>(TEXT("WeaponComp"));
	AbilityComp   = CreateDefaultSubobject<USHMAbilityComponent>(TEXT("AbilityComp"));
	BuildComp     = CreateDefaultSubobject<USHMBuildComponent>(TEXT("BuildComp"));

	// ===== 俯视角摄像机 =====
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 1500.f;
	SpringArm->SetRelativeRotation(FRotator(-70.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ASMCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 绑定死亡
	if (AttributeComp)
	{
		AttributeComp->OnDeath.AddDynamic(this, &ASMCharacter::OnOwnerDeath);
	}

	// 延迟检查武器切换绑定：等 BP 在 Possessed 里的 AddMappingContext 先完成，
	// 再查询"有没有按键映射到切换"才准（踩坑 #4 的时序教训）
	GetWorldTimerManager().SetTimer(BindingCheckTimer, this,
		&ASMCharacter::EnsureSwitchWeaponBinding, 0.5f, false);
}

void ASMCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 绑定。
	// 资产未配置时必须出声（踩坑 #15：静默跳过让"没绑上"和"逻辑坏了"无法区分）
	auto WarnIfNull = [](const UInputAction* Asset, const TCHAR* Name)
	{
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("输入资产 %s 未在蓝图里赋值——该输入不会生效"), Name);
		}
		return Asset != nullptr;
	};

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (WarnIfNull(IA_Move, TEXT("IA_Move")))
		{
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASMCharacter::OnMove);
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Completed, this, &ASMCharacter::OnMove);
		}
		if (WarnIfNull(IA_Attack, TEXT("IA_Attack")))       { EnhancedInput->BindAction(IA_Attack, ETriggerEvent::Started, this, &ASMCharacter::OnAttack); }
		if (WarnIfNull(IA_Dodge, TEXT("IA_Dodge")))         { EnhancedInput->BindAction(IA_Dodge, ETriggerEvent::Started, this, &ASMCharacter::OnDodge); }
		if (WarnIfNull(IA_SwitchWeapon, TEXT("IA_SwitchWeapon"))) { EnhancedInput->BindAction(IA_SwitchWeapon, ETriggerEvent::Started, this, &ASMCharacter::OnSwitchWeapon); }
		// 技能是后续内容，未配置不告警
		if (IA_Skill1) { EnhancedInput->BindAction(IA_Skill1, ETriggerEvent::Started, this, &ASMCharacter::OnSkill1); }
		if (IA_Skill2) { EnhancedInput->BindAction(IA_Skill2, ETriggerEvent::Started, this, &ASMCharacter::OnSkill2); }
	}
}

void ASMCharacter::EnsureSwitchWeaponBinding()
{
	// 武器切换是画像分化的输入源，不能依赖蓝图有没有人配。两种缺口都兜住：
	//   缺口①：BP 没给 IA_SwitchWeapon 赋值 → 运行时造一个 IA 并绑定
	//   缺口②：IA 有了但 IMC 里没映射按键 → 运行时补一张映射空格的 IMC
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { return; }

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Subsystem || !EnhancedInput) { return; }

	// 缺口①
	if (!IA_SwitchWeapon)
	{
		FallbackSwitchAction = NewObject<UInputAction>(this, TEXT("IA_SwitchWeapon_Fallback"));
		FallbackSwitchAction->ValueType = EInputActionValueType::Boolean;
		EnhancedInput->BindAction(FallbackSwitchAction, ETriggerEvent::Started, this, &ASMCharacter::OnSwitchWeapon);
		UE_LOG(LogTemp, Warning, TEXT("IA_SwitchWeapon 未配置——已用代码兜底创建并绑定"));
	}

	const UInputAction* SwitchAction = IA_SwitchWeapon ? IA_SwitchWeapon.Get() : FallbackSwitchAction.Get();

	// 缺口②：查这颗 IA 当前映射了哪些按键。
	// 注意不只查"有没有"，还要把键位打出来——"有映射但不是空格"（比如当年配的滚轮）
	// 和"没映射"对玩家是同一种感受，日志必须能区分
	const TArray<FKey> MappedKeys = Subsystem->QueryKeysMappedToAction(SwitchAction);

	FString KeyList;
	bool bHasSpace = false;
	for (const FKey& Key : MappedKeys)
	{
		KeyList += Key.GetDisplayName().ToString() + TEXT(" ");
		bHasSpace |= (Key == EKeys::SpaceBar);
	}
	UE_LOG(LogTemp, Log, TEXT("武器切换当前映射键位: %s"),
		MappedKeys.Num() ? *KeyList : TEXT("（无）"));

	if (!bHasSpace)
	{
		FallbackContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SwitchFallback"));
		FallbackContext->MapKey(SwitchAction, EKeys::SpaceBar);
		Subsystem->AddMappingContext(FallbackContext, /*Priority=*/10);
		UE_LOG(LogTemp, Warning, TEXT("空格未映射到武器切换——已用代码兜底补上（原映射保留仍可用）"));
	}
}

void ASMCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMouseWorldLocation();
	UpdateRotationToMouse();
}

// ============================================================================
//  移动 + 瞄准
// ============================================================================

void ASMCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!FMath::IsNearlyZero(Input.X) || !FMath::IsNearlyZero(Input.Y))
	{
		// 俯视角：X=前后(Y轴), Y=左右(X轴)
		AddMovementInput(FVector::ForwardVector, Input.X);
		AddMovementInput(FVector::RightVector, Input.Y);
	}
	// Completed 时不传值过来就会归零，不需要额外 stop
}

void ASMCharacter::OnAim(const FInputActionValue& Value)
{
	// 键鼠 twin-stick：瞄准是鼠标位置被动轮询（Tick 里的 UpdateMouseWorldLocation）
	// 不需要额外输入逻辑。保留此回调供手柄右摇杆扩展。
}

void ASMCharacter::UpdateMouseWorldLocation()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FHitResult Hit;
	PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	if (Hit.bBlockingHit)
	{
		CachedMouseWorldLocation = Hit.Location;
	}
}

void ASMCharacter::UpdateRotationToMouse()
{
	const FVector Direction = CachedMouseWorldLocation - GetActorLocation();
	if (!Direction.IsNearlyZero())
	{
		const FRotator TargetRotation = Direction.Rotation();
		SetActorRotation(FRotator(0.f, TargetRotation.Yaw, 0.f));
	}
}

// ============================================================================
//  攻击（Sprint 1：近战扇形）
// ============================================================================

void ASMCharacter::OnAttack()
{
	if (!WeaponComp) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < WeaponComp->GetActiveAttackSpeed())
	{
		return;
	}
	LastAttackTime = Now;

	// 攻击次数上报 —— 画像「Build 集中度」的主要数据源。
	// 注意：这里报的是"发起攻击"，不是"命中"。集中度衡量的是玩家倾向用什么，
	// 打不打得中是「战斗效率」维度的事，两者不能混在一个计数里。
	if (USHMEventBus* Bus = USHMEventBus::Get(this))
	{
		Bus->BroadcastSimple(SHMTags::Event_Combat_Attack, this,
			WeaponComp->GetActiveWeaponTag());
	}

	// 按当前武器的攻击模式分发——武器是数据+模式枚举，不是每把一个类（TDD §2.4）
	switch (WeaponComp->GetActiveAttackPattern())
	{
	case EAttackPattern::Ranged_Shot:
	case EAttackPattern::Ranged_Pierce:
		PerformRangedAttack();
		break;
	default:
		PerformMeleeAttack();
		break;
	}
}

void ASMCharacter::PerformMeleeAttack()
{
	const FVector     Origin   = GetActorLocation();
	const FVector     Forward  = GetActorForwardVector();
	const float       Range    = 200.f;
	const float       Angle    = 90.f;

	// 导演规则作用面：近战伤害倍率（无规则生效时为 1.0）
	float Damage = WeaponComp->GetActiveBaseDamage();
	if (const USHMDirectorCore* Director = GetGameInstance()->GetSubsystem<USHMDirectorCore>())
	{
		Damage *= Director->GetActiveRuleMultiplier(SHMTags::Rule_MeleeDamage.GetTag());
	}

	TArray<FHitResult> Hits;
	const bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		this, Origin, Origin + Forward * Range, 50.f,
		UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), // 自定义碰撞通道，TL 需在 Project Settings 建一条 Enemy 通道
		false, TArray<AActor*>(), EDrawDebugTrace::ForDuration,
		Hits, true, FLinearColor::Red, FLinearColor::Green, 0.5f
	);

	for (const FHitResult& Hit : Hits)
	{
		if (AActor* Victim = Hit.GetActor())
		{
			if (USHMAttributeComponent* VictimAttr = Victim->FindComponentByClass<USHMAttributeComponent>())
			{
				VictimAttr->ApplyDamage(Damage, this, WeaponComp->GetActiveWeaponTag());
			}
		}
	}

	// 命中停顿（Sprint 1 简单实现：播放攻击蒙太奇时挂在 AnimNotify 里暂停 ~0.03s）
	// S1 先不接入 HitStop，这个由 BP 里挂蒙太奇实现
}

void ASMCharacter::PerformRangedAttack()
{
	// 朝鼠标世界坐标的水平方向发射（twin-stick：射向与移动解耦）
	FVector Dir = CachedMouseWorldLocation - GetActorLocation();
	Dir.Z = 0.f;
	Dir = Dir.IsNearlyZero() ? GetActorForwardVector() : Dir.GetSafeNormal();

	const FVector SpawnLoc = GetActorLocation() + Dir * 80.f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 导演规则作用面：远程伤害倍率
	float Damage = WeaponComp->GetActiveBaseDamage();
	if (const USHMDirectorCore* Director = GetGameInstance()->GetSubsystem<USHMDirectorCore>())
	{
		Damage *= Director->GetActiveRuleMultiplier(SHMTags::Rule_RangedDamage.GetTag());
	}

	if (ASHMProjectile* Proj = GetWorld()->SpawnActor<ASHMProjectile>(
		ASHMProjectile::StaticClass(), SpawnLoc, Dir.Rotation(), Params))
	{
		Proj->InitProjectile(Damage, WeaponComp->GetActiveWeaponTag(), this);
	}
}

// ============================================================================
//  闪避（S2-F1 实现完整逻辑）
// ============================================================================

void ASMCharacter::OnDodge()
{
	if (!AbilityComp) return;

	const FVector InputDir = GetLastMovementInputVector();
	const FVector DodgeDir = InputDir.IsNearlyZero()
		? GetActorForwardVector()
		: InputDir.GetSafeNormal();

	const bool bDodged = AbilityComp->TryDodge(DodgeDir);

	// 冷却中按闪避也要上报（bSuccess=false）——「闪避成功率」的分母是尝试次数，
	// 只报成功的话这个维度永远是 100%
	if (USHMEventBus* Bus = USHMEventBus::Get(this))
	{
		Bus->BroadcastSimple(SHMTags::Event_Combat_Dodge, this,
			SHMTags::Ability_Dodge, FGameplayTag(), 0.f, bDodged);
	}

	if (bDodged)
	{
		// S2-F1：这里加 LaunchCharacter + 无敌帧
		LaunchCharacter(DodgeDir * 1000.f, false, false);
	}
}

// ============================================================================
//  武器切换 / 技能
// ============================================================================

void ASMCharacter::OnSwitchWeapon()
{
	if (!WeaponComp) return;

	const FGameplayTag PrevTag = WeaponComp->GetActiveWeaponTag();
	WeaponComp->SwitchWeapon();  // S2-F2 实现
	const FGameplayTag NewTag = WeaponComp->GetActiveWeaponTag();

	// 切换成功才算数（副武器没配时 SwitchWeapon 是空操作，不能计入「策略切换意愿」）
	if (PrevTag != NewTag)
	{
		if (USHMEventBus* Bus = USHMEventBus::Get(this))
		{
			Bus->BroadcastSimple(SHMTags::Event_Combat_WeaponSwitch, this, NewTag, PrevTag);
		}

		// 切换反馈：武器没有模型/图标，不提示的话玩家无法知道切换发生了——
		// "按了没反应"和"切了但看不出来"必须可区分（正式 HUD 是第五次开工的事）
		if (GEngine)
		{
			const bool bNowBow = NewTag == SHMTags::Weapon_Bow.GetTag();
			GEngine->AddOnScreenDebugMessage(1004, 2.f, bNowBow ? FColor::Yellow : FColor::Orange,
				FString::Printf(TEXT("【武器】%s"), bNowBow ? TEXT("弓（左键远程射击）") : TEXT("剑（左键近战横扫）")));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SwitchWeapon 被调用但武器未变化（副武器未配置？）"));
	}
}

void ASMCharacter::OnSkill1()
{
	if (AbilityComp)
	{
		AbilityComp->TryCastSkill(0);  // S4 实现
	}
}

void ASMCharacter::OnSkill2()
{
	if (AbilityComp)
	{
		AbilityComp->TryCastSkill(1);  // S4 实现
	}
}

// ============================================================================
//  死亡
// ============================================================================

void ASMCharacter::OnOwnerDeath()
{
	// 禁用输入
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	// 禁用移动
	GetCharacterMovement()->DisableMovement();

	// TL：这里在 BP 里播放死亡动画 + 3s 后调用 GameMode 的 Restart
	// C++ 只负责停止逻辑
}
