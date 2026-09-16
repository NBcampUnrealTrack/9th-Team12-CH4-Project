#include "Character/TDPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Combat/TDCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
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
#include "Character/TDSilhouetteComponent.h"
#include "Character/TDCharacterClassData.h"
#include "Data/TDCharacterClassRow.h"
#include "Engine/DataTable.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Skill/TDSkillComponent.h"
#include "UI/InGame/TDNameplateWidget.h"
#include "UI/Settings/TDUISettings.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

void ATDPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ATDPlayerCharacter, AutoRespawnEndServerTime, COND_OwnerOnly);
}

float ATDPlayerCharacter::GetAutoRespawnRemainingSeconds() const
{
    if (!IsDead() || AutoRespawnEndServerTime <= 0.0 || !GetWorld()) return -1.f;
    const AGameStateBase* State = GetWorld()->GetGameState();
    const double Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    return FMath::Max(0.f, static_cast<float>(AutoRespawnEndServerTime - Now));
}

ATDPlayerCharacter::ATDPlayerCharacter()
{
	//상호작용
	InteractionComponent =
	CreateDefaultSubobject<UTDInteractionComponent>(TEXT("InteractionComponent"));

	SkillComponent = CreateDefaultSubobject<UTDSkillComponent>(TEXT("SkillComponent"));

	SilhouetteComponent = CreateDefaultSubobject<UTDSilhouetteComponent>(TEXT("SilhouetteComponent"));

	// 이름표. 화면 공간이라 카메라가 어디를 보든 정면으로, 거리와 무관하게 또렷하게 그려진다.
	// 몬스터 체력바·NPC 퀘스트 마커와 같은 설정이다. 위치와 위젯은 BeginPlay 에서 넣는다.
	NameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameplateComponent"));
	NameplateComponent->SetupAttachment(GetRootComponent());
	NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateComponent->SetDrawAtDesiredSize(true);
	NameplateComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameplateComponent->SetGenerateOverlapEvents(false);
	NameplateComponent->SetCanEverAffectNavigation(false);

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
	BindClassAppearance();
}

void ATDPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트 경로. 이제야 PlayerState 와 그 안의 컴포넌트들에 접근할 수 있다.
	InitAbilityActorInfo();
	BindToStatComponent();
	BindClassAppearance();
}

void ATDPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupNameplate();
}

// ── 이름표 ────────────────────────────────────────────────

void ATDPlayerCharacter::SetupNameplate()
{
	// 데디 서버는 화면이 없다. UI 는 그리는 머신(클라·리슨 서버)에서만 만든다.
	if (IsRunningDedicatedServer() || NameplateComponent == nullptr)
	{
		return;
	}

	const TSubclassOf<UTDNameplateWidget> WidgetClass =
		GetDefault<UTDUISettings>()->NameplateWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("이름표: 위젯 클래스가 비어 있어 이름표가 뜨지 않는다 — "
			     "Project Settings > Game > TD UI > Nameplate Widget Class 를 지정할 것."));
		return;
	}

	// 캡슐 꼭대기 위로 올린다. 캡슐 크기는 BP 에서 바뀔 수 있으므로 생성자가 아니라 여기서 읽는다.
	const float HalfHeight = GetCapsuleComponent() != nullptr
		? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.f;
	NameplateComponent->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight + NameplateHeightOffset));

	NameplateComponent->SetWidgetClass(WidgetClass);

	// BeginPlay 시점엔 위젯이 아직 안 만들어졌을 수 있다. 명시적으로 만들게 한다(ATDEnemyBase 와 같다).
	NameplateComponent->InitWidget();

	if (UTDNameplateWidget* Nameplate = Cast<UTDNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
	{
		// 위젯은 자기가 누구 머리 위에 있는지 모른다 — 알려주는 것은 소유자 몫.
		Nameplate->SetTargetPawn(this);
	}
}

// ── 직업 외형 ─────────────────────────────────────────────

void ATDPlayerCharacter::BindClassAppearance()
{
	ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
	if (State == nullptr)
	{
		return;
	}

	// 직업 값이 PlayerState 보다 늦게 도착하는 경우가 있어 알림에도 걸어 둔다.
	if (AppearanceSource.Get() != State)
	{
		if (ATDPlayerState* Previous = AppearanceSource.Get())
		{
			Previous->OnCharacterClassChanged.RemoveDynamic(this, &ATDPlayerCharacter::OnAppearanceClassChanged);
		}

		State->OnCharacterClassChanged.AddUniqueDynamic(this, &ATDPlayerCharacter::OnAppearanceClassChanged);
		AppearanceSource = State;
	}

	ApplyClassAppearance(State->GetCharacterClassId());
}

void ATDPlayerCharacter::OnAppearanceClassChanged(FName NewClassId)
{
	ApplyClassAppearance(NewClassId);
}

void ATDPlayerCharacter::ApplyClassAppearance(FName ClassId)
{
	UPaperZDAnimationComponent* Animation = GetAnimationComponent();
	if (ClassId.IsNone() || Animation == nullptr)
	{
		return;
	}

	// 파티창·HUD 초상화와 같은 경로로 읽는다(UTDPlayerStatsViewModel). 직업 → 그림의 짝이 한 곳에 모인다.
	const UDataTable* ClassTable = UTDCharacterClassSettings::Get()->ClassTable.LoadSynchronous();
	const FTDCharacterClassRow* Row = ClassTable != nullptr
		? ClassTable->FindRow<FTDCharacterClassRow>(ClassId, TEXT("ATDPlayerCharacter::ApplyClassAppearance"), false)
		: nullptr;
	const UTDCharacterClassData* Visuals = Row != nullptr ? Row->VisualData.LoadSynchronous() : nullptr;
	const TSubclassOf<UPaperZDAnimInstance> AnimClass = Visuals != nullptr ? Visuals->AnimInstanceClass.LoadSynchronous() : nullptr;

	if (AnimClass == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("외형: '%s' 의 애니메이션을 찾지 못해 기본 외형으로 남는다 — "
			     "DT_CharacterClass 행 · VisualData · 그 에셋의 AnimInstanceClass 를 확인할 것."),
			*ClassId.ToString());
		return;
	}

	// 다시 넣으면 애니메이션 인스턴스가 새로 만들어져 재생 중이던 동작이 끊긴다.
	if (Animation->GetAnimInstanceClass() == AnimClass)
	{
		return;
	}

	Animation->SetAnimInstanceClass(AnimClass);

	UE_LOG(LogTemp, Log, TEXT("외형 적용(%s): %s — %s → %s"),
		GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버"),
		*GetName(), *ClassId.ToString(), *AnimClass->GetName());
}

void ATDPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 로컬 컨트롤러가 빙의한 뒤에만 불리는 자리라 "내 캐릭터" 판정을 겸한다.
	SilhouetteComponent->EnableForLocalPlayer();

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

		// Started 는 "안 눌림 → 눌림" 으로 바뀌는 순간에만 온다. 시전을 끊는 것은 이쪽이다 —
		// Triggered 로 끊으면 이동 중에 스킬을 누르는 순간 이미 눌려 있던 키가
		// 다음 프레임에 바로 취소시켜 버린다.
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Started, this, &ATDPlayerCharacter::MoveStarted);
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

	// 시전 중이면 그 스킬이 허락한 만큼만 움직인다.
	//
	// 서버에도 같은 감시가 있지만(UTDSkillComponent::TickComponent) 그쪽은 안전망이다.
	// 여기서 먼저 막아야 클라이언트 화면이 먼저 반응하고, 서버가 뒤늦게 끊어
	// "움직였더니 스킬이 취소됐다" 가 한 박자 늦게 보이는 일이 없다.
	const ETDSkillCastMovement CastMovement =
		SkillComponent ? SkillComponent->GetCastMovement() : ETDSkillCastMovement::Free;

	if (CastMovement == ETDSkillCastMovement::TurnOnly)
	{
		// 제자리에서 방향만 바꾼다. bOrientRotationToMovement 는 속도를 보고 도는데
		// 제자리에서는 속도가 없으므로 직접 돌려야 한다.
		const FVector Direction =
			FVector::ForwardVector * Axis.Y + FVector::RightVector * Axis.X;
		const FRotator Facing = Direction.Rotation();

		SetActorRotation(Facing);

		// 히트박스 방향도 함께 돌린다. 그쪽은 마지막 이동 방향을 쓰는데(UTDCombatComponent)
		// 그 값은 속도에서 나오므로 제자리 회전으로는 갱신되지 않는다.
		// 이 줄이 없으면 디버그 상자가 처음 방향에 그대로 서 있다.
		if (UTDCombatComponent* Combat = GetCombatComponent())
		{
			Combat->SetFacingDirection(Direction);
		}

		// 서버에도 알린다. 판정은 서버가 자기 값으로 하므로 여기서만 돌리면
		// 화면에서는 돌았는데 광선은 처음 방향으로 계속 나간다.
		SkillComponent->ServerSetCastFacing(Facing);
		return;
	}

	if (CastMovement == ETDSkillCastMovement::Locked)
	{
		// 제자리에 선다. 취소는 여기가 아니라 MoveStarted 가 맡는다 —
		// 이동 중에 스킬을 누른 경우, 아직 쥐고 있는 키로 끊기면 안 되기 때문이다.
		return;
	}

	// 월드 축 기준이다. 탑다운에서는 카메라가 어디를 보든 "위" 키가 같은 방향이어야 한다.
	// 이동 자체는 CharacterMovementComponent 가 예측·복제까지 처리하므로 RPC 를 만들지 않는다(D42).
	AddMovementInput(FVector::ForwardVector, Axis.Y);
	AddMovementInput(FVector::RightVector, Axis.X);
}

void ATDPlayerCharacter::MoveStarted()
{
	// 이동 키를 **새로** 눌렀을 때만 온다. 이동 중에 스킬을 쓴 경우 그 키는 이미 눌려 있으므로
	// 여기로 오지 않는다 — 그래서 "움직이던 중에 시전 → 키를 뗐다 다시 눌러야 취소" 가 된다.
	if (SkillComponent == nullptr
		|| SkillComponent->GetCastMovement() != ETDSkillCastMovement::Locked)
	{
		return;
	}

	// TurnOnly 는 여기 오지 않는다. 겨냥을 계속 바꾸는 것이 그 스킬의 목적이라
	// 새로 누른 입력도 회전으로만 쓰인다.
	SkillComponent->ServerCancelCast();
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
	if (SkillComponent == nullptr)
	{
		return;
	}

	// 배열 인덱스는 0부터지만 DT_Skill 의 SlotIndex 는 1부터다. 시트에서 Q·W·E 를
	// 1·2·3 으로 읽는 편이 자연스러워 그렇게 두었고, 변환은 여기 한 곳에서만 한다.
	//
	// 클라이언트는 "몇 번을 눌렀다"만 보낸다. 쿨타임·마나·사거리는 전부 서버가 판정한다 —
	// 평타(Attack)와 같은 구조다.
	SkillComponent->ServerUseSkillSlot(SkillIndex + 1);
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

	// 죽는 순간 시전을 접는다. 놔두면 시체가 정신집중을 마저 채워 피해를 넣는다.
	if (SkillComponent != nullptr)
	{
		SkillComponent->CancelCast();
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

    AutoRespawnEndServerTime = GetWorld()->GetTimeSeconds() + Delay;
    ForceNetUpdate();

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
        AutoRespawnEndServerTime = 0.0;
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

