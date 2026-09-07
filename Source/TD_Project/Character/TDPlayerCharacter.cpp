#include "Character/TDPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Combat/TDCombatComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Game/TDGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "Items/TDQuickSlotComponent.h"
#include "Player/TDPlayerState.h"
#include "Interaction/TDInteractionComponent.h"

ATDPlayerCharacter::ATDPlayerCharacter()
{
	//상호작용
	InteractionComponent =
	CreateDefaultSubobject<UTDInteractionComponent>(TEXT("InteractionComponent"));
	
	// 이동 방향으로 캐릭터가 돌아야 PaperZD 가 4방향 스프라이트 중 맞는 것을 고른다.
	// 컨트롤러 회전을 따라가면 카메라를 돌릴 때 캐릭터가 같이 돌아 방향이 어긋난다.
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;

		// 2D 스프라이트는 회전 중간 프레임이 없다. 천천히 돌면 어중간한 각도에서
		// 어느 방향 스프라이트를 쓸지 계속 바뀌며 깜빡인다. 즉시 돌게 한다.
		Movement->RotationRate = FRotator(0.f, 2000.f, 0.f);
	}
}

UTDStatComponent* ATDPlayerCharacter::GetStatComponent() const
{
	// 클라이언트에서는 PlayerState 복제가 끝나기 전까지 nullptr 이 나온다.
	// 오류가 아니라 정상 상태이므로, 호출하는 쪽이 결과를 확인해야 한다.
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetStatComponent() : nullptr;
}

UTDProgressionComponent* ATDPlayerCharacter::GetProgressionComponent() const
{
	// 레벨과 포인트 분배도 PlayerState 에 있다. 리스폰으로 이 액터가 파괴돼도 남아야 하기 때문.
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetProgressionComponent() : nullptr;
}

UAbilitySystemComponent* ATDPlayerCharacter::GetAbilitySystemComponent() const
{
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ATDPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 경로. 컨트롤러가 빙의하면서 PlayerState 연결이 끝난 시점이다.
	InitAbilityActorInfo();
	BindToStatComponent();
}

void ATDPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트 경로. 이제야 PlayerState 와 그 안의 컴포넌트들에 접근할 수 있다.
	InitAbilityActorInfo();
	BindToStatComponent();
}

void ATDPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 이 함수는 컨트롤러가 빙의를 마친 뒤에 불린다. LocalPlayer 가 준비돼 있으므로
	// 매핑 컨텍스트를 여기서 켜도 안전하다.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext != nullptr)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);

				// 사용자 설정에 **따로 등록해야** 리매핑 목록에 나온다.
				// AddMappingContext 는 입력을 켜기만 할 뿐이고, 이 호출이
				// IMC 안의 mappable 매핑을 훑어 키 프로필에 항목을 만든다.
				//
				// 빠뜨리면 IA 에 Name 을 제대로 넣어도 GetKeyMappings 가 빈 배열을 돌려준다 —
				// 조작은 되는데 설정 화면만 비어 있는 상태가 된다.
				//
				// **주의**: 등록은 이름 단위다. 하나의 IA 에 여러 키가 붙어 있는데
				// 전부 같은 이름을 물려받으면(Inherit) 매핑끼리 덮어써져 입력이 깨진다.
				// IA_Move 의 상하좌우처럼 축을 나눠 받는 액션은 IMC 에서 매핑마다
				// Override Settings 로 다른 이름을 줘야 한다(§11-H).
				if (UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings())
				{
					UserSettings->RegisterInputMappingContext(DefaultMappingContext);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s: DefaultMappingContext 가 비어 있어 입력이 동작하지 않는다. "
						 "BP_Player 에서 IMC 를 지정할 것."), *GetName());
			}
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInput == nullptr)
	{
		// 프로젝트 설정의 Default Input Component Class 가 Enhanced 가 아니면 여기로 온다.
		UE_LOG(LogTemp, Warning,
			TEXT("%s: EnhancedInputComponent 가 아니다. Project Settings > Input 을 확인할 것."),
			*GetName());
		return;
	}

	// ── 바인딩 목록 ───────────────────────────────────────
	// 새 조작은 여기에 한 줄 추가한다. 액션이 비어 있으면 그 줄만 건너뛰므로,
	// 에셋을 아직 안 만든 조작이 있어도 나머지는 정상 동작한다.

	if (MoveAction != nullptr)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATDPlayerCharacter::Move);
	}

	if (JumpAction != nullptr)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ATDPlayerCharacter::StartJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATDPlayerCharacter::StopJump);
	}

	if (AttackAction != nullptr)
	{
		EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ATDPlayerCharacter::Attack);
	}
	
	if (AttackAction != nullptr)
	{
		EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ATDPlayerCharacter::Attack);
	}

	if (InteractAction != nullptr)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ATDPlayerCharacter::Interact);
	}

	// 퀵슬롯·스킬은 번호를 payload 로 넘겨 핸들러 하나로 처리한다.
	// 슬롯마다 함수를 만들면 똑같은 내용이 6개, 3개씩 늘어선다.
	for (int32 SlotIndex = 0; SlotIndex < QuickSlotActions.Num(); ++SlotIndex)
	{
		if (QuickSlotActions[SlotIndex] != nullptr)
		{
			EnhancedInput->BindAction(QuickSlotActions[SlotIndex], ETriggerEvent::Started,
				this, &ATDPlayerCharacter::UseQuickSlot, SlotIndex);
		}
	}

	for (int32 SkillIndex = 0; SkillIndex < SkillActions.Num(); ++SkillIndex)
	{
		if (SkillActions[SkillIndex] != nullptr)
		{
			EnhancedInput->BindAction(SkillActions[SkillIndex], ETriggerEvent::Started,
				this, &ATDPlayerCharacter::UseSkill, SkillIndex);
		}
	}
}

// ── 핸들러 ────────────────────────────────────────────────

void ATDPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// TD.InputDebug 1 로 켠다. 어느 키를 눌러도 같은 값이 나오면 IMC 문제다 —
	// Axis2D 매핑에는 키마다 Negate·Swizzle 모디파이어가 필요하고,
	// 없으면 W·A·S·D 가 전부 X 축 양수로 들어온다.
	static const IConsoleVariable* InputDebugCVar =
		IConsoleManager::Get().FindConsoleVariable(TEXT("TD.InputDebug"));

	if (InputDebugCVar != nullptr && InputDebugCVar->GetInt() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("이동 입력: X=%.2f  Y=%.2f"), Axis.X, Axis.Y);
	}
#endif

	// 월드 축 기준이다. 탑다운에서는 카메라가 어디를 보든 "위" 키가 같은 방향이어야 한다.
	// 이동 자체는 CharacterMovementComponent 가 예측·복제까지 처리하므로 RPC 를 만들지 않는다(D42).
	AddMovementInput(FVector::ForwardVector, Axis.Y);
	AddMovementInput(FVector::RightVector, Axis.X);
}

void ATDPlayerCharacter::StartJump()
{
	// ACharacter 가 제공하는 경로다. 복제와 예측이 이미 붙어 있다.
	Jump();
}

void ATDPlayerCharacter::StopJump()
{
	StopJumping();
}

void ATDPlayerCharacter::Attack()
{
	if (UTDCombatComponent* Combat = GetCombatComponent())
	{
		// 클라이언트는 요청만 보낸다. 쿨타임·히트박스·데미지는 전부 서버가 판정한다.
		Combat->ServerRequestAttack();
	}
}

void ATDPlayerCharacter::Interact()
{
	if (IsDead() || InteractionComponent == nullptr)
	{
		return;
	}

	InteractionComponent->RequestInteract();
}

void ATDPlayerCharacter::UseQuickSlot(int32 SlotIndex)
{
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	UTDQuickSlotComponent* QuickSlots =
		TDPlayerState ? TDPlayerState->GetQuickSlotComponent() : nullptr;

	if (QuickSlots == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("퀵슬롯 %d 입력 — QuickSlotComponent 가 없다. 캐릭터를 먼저 선택할 것."),
			SlotIndex + 1);
		return;
	}

	// 키가 실제로 도착했는지 남긴다. 슬롯이 비어 있으면 서버가 조용히 무시하므로
	// 이 로그가 없으면 "키가 안 먹는 것" 과 "슬롯이 비어 있는 것" 을 구분할 수 없다.
	UE_LOG(LogTemp, Log, TEXT("퀵슬롯 %d 입력."), SlotIndex + 1);

	// 클라이언트는 "몇 번을 눌렀다"만 보낸다. 그 자리에 무엇이 있는지, 쓸 수 있는지는
	// 서버가 자기 배열을 보고 판단한다 — 슬롯 내용을 함께 보내면 위조할 수 있다(D56 과 같은 원칙).
	QuickSlots->ServerUseSlot(SlotIndex);
}

void ATDPlayerCharacter::UseSkill(int32 SkillIndex)
{
	// S10 에서 어빌리티 발동으로 바꾼다. 그때도 클라이언트는 "몇 번을 눌렀다"만 보내고
	// 쿨타임·마나·사거리는 서버가 판정한다 — 평타(Attack)와 같은 구조다.
	UE_LOG(LogTemp, Log, TEXT("스킬 %d 입력 — 스킬 시스템이 아직 없다."), SkillIndex + 1);
}

void ATDPlayerCharacter::InitAbilityActorInfo()
{
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = TDPlayerState->GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// Owner 는 ASC 를 소유한 액터(PlayerState), Avatar 는 월드에서 그것을 대신하는 액터(캐릭터).
	// 둘을 나누는 이유는 수명이 다르기 때문이다 — 캐릭터는 죽으면 사라지지만 PlayerState 는 남는다.
	ASC->InitAbilityActorInfo(TDPlayerState, this);
}

// ── 사망·부활 ─────────────────────────────────────────────

void ATDPlayerCharacter::HandleDeath()
{
	Super::HandleDeath();

	SetDeadState(true);

	// 자동 부활은 서버가 건다. 버튼(ServerRequestRespawn)이 먼저 눌리면
	// GameMode 가 RespawnPlayer 안에서 이 타이머를 지운다.
	if (!HasAuthority())
	{
		return;
	}

	const ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}

	const float Delay = GameMode->GetAutoRespawnSeconds();
	if (Delay <= 0.f)
	{
		// 0 이면 자동 부활을 쓰지 않는다는 뜻이다. 버튼으로만 되살아난다.
		return;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetController());
	if (OwningController == nullptr)
	{
		return;
	}

	// 약한 참조로 잡는다. 타이머가 도는 동안 접속을 끊으면 대상이 사라진다.
	TWeakObjectPtr<APlayerController> WeakController(OwningController);
	TWeakObjectPtr<UWorld> WeakWorld(GetWorld());

	GetWorldTimerManager().SetTimer(AutoRespawnTimerHandle,
		[WeakController, WeakWorld]()
		{
			if (!WeakController.IsValid() || !WeakWorld.IsValid())
			{
				return;
			}

			if (ATDGameMode* TimerGameMode = WeakWorld->GetAuthGameMode<ATDGameMode>())
			{
				TimerGameMode->RespawnPlayer(WeakController.Get());
			}
		},
		Delay, /*bLoop=*/ false);
}

void ATDPlayerCharacter::HandleRespawn()
{
	Super::HandleRespawn();

	SetDeadState(false);

	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(AutoRespawnTimerHandle);
	}
}

void ATDPlayerCharacter::SetDeadState(bool bDead)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bDead)
		{
			// 입력만 막고 이동을 안 멈추면 죽는 순간의 속도로 계속 미끄러진다.
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	// 입력 차단은 로컬 컨트롤러에서만 의미가 있다. 서버의 원격 컨트롤러에는 입력이 없다.
	if (APlayerController* OwningController = Cast<APlayerController>(GetController()))
	{
		if (bDead)
		{
			DisableInput(OwningController);
		}
		else
		{
			EnableInput(OwningController);
		}
	}
}

