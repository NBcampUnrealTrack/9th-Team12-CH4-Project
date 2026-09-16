#pragma once

#include "CoreMinimal.h"
#include "Character/TDCharacterBase.h"
#include "TDPlayerCharacter.generated.h"

class ATDPlayerState;
class UInputAction;
class UInputMappingContext;
class USoundBase;
class UTDInteractionComponent;
class UTDSilhouetteComponent;
class UTDSkillComponent;
class UWidgetComponent;
struct FInputActionValue;

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 스탯 컴포넌트를 소유하지 않는다. 리스폰 시 이 액터는 파괴되므로,
 * 스탯은 PlayerState 쪽에 있고 여기서는 경로만 이어준다.
 */
UCLASS()
class TD_PROJECT_API ATDPlayerCharacter : public ATDCharacterBase
{
	GENERATED_BODY()

public:
	ATDPlayerCharacter();

	virtual UTDStatComponent* GetStatComponent() const override;

	virtual UTDProgressionComponent* GetProgressionComponent() const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 액티브 스킬의 시전을 담당한다. 이동 제약을 Move 가 여기에 물어본다. */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	UTDSkillComponent* GetSkillComponent() const { return SkillComponent; }

	// ── GAS 초기화 ────────────────────────────────────────
	// ASC 는 자기가 누구에게 붙었는지(Owner)와 어떤 액터를 대신하는지(Avatar)를 알아야 한다.
	// 서버는 컨트롤러가 빙의할 때, 클라이언트는 PlayerState 복제가 끝날 때 그 정보가 갖춰진다.
	//
	// **둘 중 하나만 하면 반드시 문제가 생긴다.**
	// 서버에서만 하면 클라이언트에서 어트리뷰트가 붙지 않고,
	// 클라에서만 하면 서버 계산이 반영되지 않는다. GAS 에서 가장 흔한 사고다.

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnRep_PlayerState() override;

	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ── 사망·부활 ─────────────────────────────────────────

	/**
	 * 조작을 막고 그 자리에 멈춘다. 시체를 파괴하지 않는 이유는 D11 과 같다 —
	 * 스탯과 장비는 PlayerState 에 있으므로 Pawn 을 지웠다 다시 만들 이유가 없고,
	 * 그러면 사망 애니메이션을 보여줄 대상도 사라진다.
	 *
	 * 서버는 여기서 자동 부활 타이머를 건다.
	 */
	virtual void HandleDeath() override;

	/** 조작을 되돌린다. 체력 회복과 위치 이동은 GameMode 가 맡는다. */
	virtual void HandleRespawn() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Display only; -1 means no automatic respawn is scheduled. */
    UFUNCTION(BlueprintPure, Category="TD|Respawn")
    float GetAutoRespawnRemainingSeconds() const;

	// ══ 조작 ═══════════════════════════════════════════════════════════════
	//
	// 조작과 관련된 것은 전부 여기 모여 있다. 새 조작을 추가할 때 볼 곳은 세 군데다.
	//
	//   1. 아래 액션 목록에 UInputAction 프로퍼티를 추가
	//   2. 그 아래 핸들러 블록에 함수를 추가
	//   3. .cpp 의 SetupPlayerInputComponent 안 "바인딩 목록" 에 한 줄 추가
	//
	// 에셋은 코드에 박지 않는다(D45). BP_Player 에서 지정하며, 비어 있으면
	// 그 조작만 조용히 빠지고 나머지는 정상 동작한다.
	//
	// Enhanced Input 만 쓴다(D44). BindAxisKey 같은 레거시 경로는 리매핑이 안 된다.

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 빙의한 뒤 활성화할 입력 묶음. BP_Player 에서 IMC_* 를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// ── 액션 목록 ─────────────────────────────────────────

	/** 이동. **Axis2D** 여야 한다 — X 가 좌우, Y 가 앞뒤다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input|Actions")
	TObjectPtr<UInputAction> MoveAction;

	/** 점프. Digital(bool). 누르는 동안 유지되면 높이 뛴다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input|Actions")
	TObjectPtr<UInputAction> JumpAction;

	/** 평타. Digital(bool). 실제 판정과 쿨타임은 UTDCombatComponent 가 서버에서 처리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input|Actions")
	TObjectPtr<UInputAction> AttackAction;

	
	/**
	 * 퀵슬롯 1~6. 배열 인덱스가 곧 슬롯 번호다.
	 *
	 * 슬롯마다 액션을 따로 두는 이유는 **키를 각각 바꿀 수 있어야** 하기 때문이다.
	 * 액션 하나에 여러 키를 매핑하면 어느 키가 눌렸는지 구분할 수 없다.
	 *
	 * 비어 있는 칸은 그 슬롯만 조용히 빠지고 나머지는 동작한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input|Actions")
	TArray<TObjectPtr<UInputAction>> QuickSlotActions;

	/**
	 * 스킬 1~3(Q·W·E). 퀵슬롯과 나눠 둔 이유는 성격이 달라서다 —
	 * 퀵슬롯은 무엇을 넣을지 플레이어가 정하지만, 스킬은 직업이 정한다.
	 *
	 * 배열 인덱스 0·1·2 가 DT_Skill 의 SlotIndex 1·2·3 에 대응한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input|Actions")
	TArray<TObjectPtr<UInputAction>> SkillActions;

	//상호작용 F
	UPROPERTY(EditDefaultsOnly, Category="TD|Input|Actions")
	TObjectPtr<UInputAction> InteractAction;
	
	// ── 핸들러 ────────────────────────────────────────────

	/**
	 * 이동. 컨트롤러 회전이 아니라 월드 축을 기준으로 움직인다 —
	 * 탑다운에서는 카메라가 어디를 보든 "위" 키가 항상 같은 방향이어야 하기 때문이다.
	 */
	void Move(const FInputActionValue& Value);

	/**
	 * 이동 키를 **새로** 눌렀을 때. 이동 자체는 Move 가 하고, 여기서는 시전 취소만 판단한다.
	 *
	 * Move(Triggered)에서 취소하면 이동 중에 스킬을 누르는 순간 쥐고 있던 키가
	 * 다음 프레임에 바로 끊어 버려서, 뛰면서는 스킬을 아예 못 쓰게 된다.
	 */
	void MoveStarted();

	void StartJump();
	void StopJump();

	/** 서버에 공격을 요청한다. 검증·쿨타임·히트박스는 전부 서버 몫이다. */
	void Attack();

	/**
	 * 퀵슬롯 사용. 슬롯 번호를 **바인딩 시점에 payload 로 넘겨** 핸들러 하나로 처리한다.
	 * 슬롯마다 함수를 만들면 6개가 똑같은 내용으로 늘어선다.
	 */
	void UseQuickSlot(int32 SlotIndex);

	/** 스킬 사용. 서버에 "몇 번을 눌렀다" 만 보낸다. 판단은 전부 서버 몫이다. */
	void UseSkill(int32 SkillIndex);

	// 상호작용
	void Interact();
	// ══════════════════════════════════════════════════════════════════════

private:
	/** 서버가 승인한 기본 공격의 기존 멀티캐스트 이벤트에서만 재생한다. */
	UFUNCTION()
	void HandleBasicAttackSoundStarted();

	/** 직업 데이터 적용 때 로드하여 보관한다. 공격마다 에셋을 조회하지 않는다. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedBasicAttackSound;

	/** 서버·클라 양쪽에서 불린다. 여러 번 호출돼도 안전하다. */
	void InitAbilityActorInfo();

	// ── 직업 외형 ─────────────────────────────────────────
	// 직업 → 애니메이션 BP 는 DT_CharacterClass → VisualData(UTDCharacterClassData) 에서 읽는다.
	// PlayerState 가 확실히 붙는 두 순간(PossessedBy · OnRep_PlayerState)에 걸어서, 스폰·복제 순서와
	// 무관하게 한 번은 적용된다. 애니메이션 종류는 복제되지 않으므로 각 머신이 스스로 넣는다.

	/** PlayerState 의 직업 변경 알림에 연결하고 지금 직업을 적용한다. 여러 번 불려도 한 번만 연결한다. */
	void BindClassAppearance();

	/**
	 * 직업 변경 알림 수신. 이름을 HandleClassChanged 로 두면 BP_Player 의 같은 이름 커스텀 이벤트와
	 * 겹쳐 BP 컴파일이 깨진다(2026-09-16) — UFUNCTION 은 자식 BP 의 이벤트·함수 이름과 겹치면 안 된다.
	 */
	UFUNCTION()
	void OnAppearanceClassChanged(FName NewClassId);

	/** 직업이 아직 없으면(None) 건너뛴다 — 도착하면 알림으로 다시 온다. 같은 애니메이션이면 다시 넣지 않는다. */
	void ApplyClassAppearance(FName ClassId);

	/** 알림을 연결한 PlayerState. 바뀌면 옛 연결을 끊는다. */
	TWeakObjectPtr<ATDPlayerState> AppearanceSource;

	// ── 이름표 ────────────────────────────────────────────

	/**
	 * 머리 위 이름표를 켠다. 위젯 클래스는 프로젝트 설정(TD UI)에서 읽는다 —
	 * 에셋을 코드에 박지 않고, BP_Player 를 고치지 않으려는 것이다.
	 *
	 * 내 캐릭터·남의 캐릭터 모두 켠다. 이름 값은 PlayerState 가 복제해 주므로
	 * 각 머신이 자기 화면의 모든 캐릭터에 대해 스스로 만든다.
	 */
	void SetupNameplate();

	/**
	 * 죽은 뒤 조작을 막고 이동을 멈춘다. 되살아나면 반대로 되돌린다.
	 *
	 * 입력만 막고 이동을 안 멈추면 죽는 순간의 속도로 계속 미끄러진다.
	 */
	void SetDeadState(bool bDead);

	/** 자동 부활 타이머. 서버에서만 돈다. */
	FTimerHandle AutoRespawnTimerHandle;

    UPROPERTY(Replicated)
    double AutoRespawnEndServerTime = 0.0;
	
	
	/**  상호작용
	 * NPC, 상자, 아이템 등 F키 상호작용을 처리한다.
	 *
	 * 검색과 Server RPC는 이 컴포넌트에 모으고,
	 * 캐릭터는 입력만 전달한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|Interaction",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTDInteractionComponent> InteractionComponent;

	/**
	 * 액티브 스킬. 평타(UTDCombatComponent)와 마찬가지로 Pawn 에 둔다 —
	 * 쿨타임과 시전 상태는 아바타의 것이다. 스킬 레벨과 테이블은
	 * PlayerState 의 UTDProgressionComponent 를 거친다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|Skill",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTDSkillComponent> SkillComponent;

	/** 벽 뒤에 가려진 내 캐릭터의 윤곽. 로컬 플레이어에서만 켜진다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Presentation")
	TObjectPtr<UTDSilhouetteComponent> SilhouetteComponent;

	/** 머리 위 이름표. 데디케이티드 서버에서는 만들지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Presentation")
	TObjectPtr<UWidgetComponent> NameplateComponent;

	/** 이름표를 캡슐 꼭대기에서 얼마나 더 올릴지(cm). 데미지 텍스트와 겹치면 이 값을 조정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Presentation")
	float NameplateHeightOffset = 20.f;
};
