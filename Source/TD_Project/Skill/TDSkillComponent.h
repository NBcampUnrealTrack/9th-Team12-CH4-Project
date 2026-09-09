#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Skill/TDSkillTypes.h"
#include "TDSkillComponent.generated.h"

class ATDCharacterBase;
class UTDProgressionComponent;
struct FTDSkillRow;

/**
 * 시전이 시작됐을 때. 캐스팅 바·시전 애니메이션·이펙트가 구독한다. 전 머신에서 불린다.
 *
 * CastTime 이 0 이면 기다림이 없는 즉발이므로 바를 그리지 않아도 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTDOnSkillCastStarted,
	FName, SkillId, float, CastTime, ETDSkillCastType, CastType);

/**
 * 시전이 끝났을 때. 바를 닫고 이펙트를 멈춘다. 전 머신에서 불린다.
 *
 * bFired 가 false 면 한 번도 발동하지 못하고 끊긴 것이다 — 마나도 쿨도 쓰지 않았다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnSkillCastEnded, FName, SkillId, bool, bFired);

/**
 * 액티브 스킬의 시전을 담당하는 컴포넌트. Pawn 에 붙는다.
 *
 * 쿨타임이 아바타의 것이라 UTDCombatComponent 와 같은 자리에 둔다. 반대로 스킬 레벨과
 * 테이블 조회는 UTDProgressionComponent(PlayerState) 를 거친다 — 같은 테이블을 두
 * 컴포넌트가 각자 들면 한쪽만 지정해 놓고 왜 안 되는지 찾게 된다.
 *
 * ── 마나와 쿨을 언제 치르는가 ──
 * 누를 때는 **검사만** 하고, 실제 소모는 첫 판정 때, 쿨은 시전이 끝날 때다.
 * 끊기면 아무것도 쓰지 않은 것이 되므로 되돌리는 코드가 필요 없다 —
 * 롤백을 만들면 되돌리기를 빠뜨리는 경로가 반드시 하나 생긴다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDSkillComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── 입력 ──────────────────────────────────────────────

	/**
	 * 그 자리(1~3 = Q·W·E)의 스킬을 쓴다.
	 *
	 * 클라이언트는 "몇 번을 눌렀다" 만 보낸다. 그 자리에 어느 스킬이 있는지, 쓸 수 있는지는
	 * 서버가 자기 데이터로 판단한다 — 스킬 ID 를 함께 보내면 위조할 수 있다(D56).
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Skill")
	void ServerUseSkillSlot(int32 SlotIndex);

	/** 클라이언트가 시전을 접는다. 이동 취소(Locked)와 UI 의 취소 버튼이 쓴다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Skill")
	void ServerCancelCast();

	/**
	 * 시전을 접는다. **서버 전용** — 스턴·넉백·사망처럼 서버가 판단하는 것들이 부른다.
	 *
	 * CC 를 붙이는 쪽이 이 함수만 부르면 되도록 열어 둔다. 취소 조건을 스킬 컴포넌트가
	 * 다 알 필요가 없고, 나중에 수면·속박이 생겨도 같은 함수를 부르면 끝이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Skill")
	void CancelCast();

	/**
	 * TurnOnly 스킬을 쓰는 동안의 바라보는 방향.
	 *
	 * 클라이언트가 제자리에서 회전할 때 서버에도 알려야 판정 방향이 맞는다. 매 프레임이
	 * 아니라 방향이 실제로 바뀔 때만 보내며, 마지막 값만 맞으면 되므로 Unreliable 이다.
	 */
	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "TD|Skill")
	void ServerSetCastFacing(FRotator Facing);

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	bool IsCasting() const { return !CastingSkillId.IsNone(); }

	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	FName GetCastingSkillId() const { return CastingSkillId; }

	/**
	 * 지금 시전 중인 스킬의 이동 제약. 시전 중이 아니면 Free.
	 *
	 * 클라이언트에서도 정확하다 — 시전 시작·종료가 Multicast 로 전 머신에 전달되고,
	 * DT_Skill 은 모든 머신이 들고 있기 때문이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	ETDSkillCastMovement GetCastMovement() const;

	/** 남은 쿨(초). 0 이면 쓸 수 있다. 아이콘 위 부채꼴이 이 값을 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	float GetCooldownRemaining(FName SkillId) const;

	/** 위와 같지만 Q·W·E 자리로 묻는다. HUD 가 슬롯 번호만 알고 있을 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Skill")
	float GetCooldownRemainingForSlot(int32 SlotIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|Skill")
	FTDOnSkillCastStarted OnCastStarted;

	UPROPERTY(BlueprintAssignable, Category = "TD|Skill")
	FTDOnSkillCastEnded OnCastEnded;

protected:
	/**
	 * 판정 범위를 그려준다. 스프라이트가 없는 지금은 사거리를 눈으로 맞추는 유일한 방법이다.
	 * 평타의 bDrawDebugHitBox 와 같은 역할.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Skill")
	bool bDrawDebugShape = true;

	/**
	 * 시전 중 이동으로 볼 속도(cm/s). 이보다 빠르면 끊는다.
	 *
	 * 0 으로 두지 않는 이유는 지면 마찰이나 물리 밀림으로 미세한 속도가 남아 있을 때
	 * 가만히 서 있는데도 끊기기 때문이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Skill", meta = (ClampMin = "0"))
	float MoveCancelSpeed = 10.f;

	/**
	 * 시전 시작 직후 이동 감시를 쉬는 시간(초).
	 *
	 * 클라이언트는 Multicast 가 도착해야 시전 중인 것을 알고 이동 입력을 막는다. 그 사이에는
	 * 이동 요청이 계속 오므로, 이 유예가 없으면 뛰면서 쓴 스킬이 서버에서 즉시 끊긴다.
	 * 왕복 지연보다 넉넉해야 하며, 그 안에 일어나는 이동은 취소로 보지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Skill", meta = (ClampMin = "0"))
	float MoveCancelGraceSeconds = 0.25f;

	// 시전의 시작과 끝은 전 머신이 알아야 한다. 연출뿐 아니라 클라이언트의 이동 제약과
	// 쿨 표시가 이 두 신호로 굴러간다. 초당 몇 번 수준이라 Reliable 로 둔다 —
	// 놓치면 캐스팅 바가 안 닫히거나 쿨이 안 보이는 상태로 남는다.

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnCastStarted(FName SkillId, float CastTime, ETDSkillCastType CastType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnCastEnded(FName SkillId, bool bFired, float Cooldown);

private:
	ATDCharacterBase* GetOwnerCharacter() const;
	UTDProgressionComponent* GetProgression() const;

	/**
	 * 맞았을 때. 경직이 걸렸으면 시전을 끊는다.
	 *
	 * ATDCharacterBase::ReceiveHit 이 평타 스윙을 CancelAttack 으로 끊는 것과 짝이다.
	 * 그쪽을 고쳐 시전까지 끊게 하지 않고 여기서 구독하는 이유는, 스킬을 모르는
	 * 베이스 클래스가 스킬 컴포넌트를 알아야 하는 상황을 만들지 않기 위해서다.
	 */
	void HandleDamaged(AActor* Attacker, float Damage, bool bCritical);

	FDelegateHandle DamagedHandle;

	/** 지금 시전 중인 스킬의 행. 시전 중이 아니면 nullptr. */
	const FTDSkillRow* GetCastingRow() const;

	/** 시전을 시작해도 되는가. 실패하면 사유를 로그로 남긴다. 서버 전용. */
	bool CanStartCast(FName SkillId, const FTDSkillRow& Row) const;

	/** 기다림이 끝났다. 첫 판정을 내고, 정신집중이면 반복 타이머를 건다. */
	void OnCastFinished();

	/** 한 번의 판정. 정신집중이면 여러 번 불린다. */
	void FireOnce();

	/** 시전 상태를 지우고 전 머신에 끝을 알린다. bFired 가 false 면 쿨도 마나도 없다. */
	void EndCast(bool bFired);

	/** 모양에 따라 대상을 모은다. Skill.Shape.Self 는 빈 배열이다. */
	TArray<AActor*> GatherTargets(const FTDSkillRow& Row) const;

	/** 효과 하나를 적용한다. 피해는 대상에게, 회복은 시전자에게 간다. */
	void ApplyEffect(FGameplayTag EffectTag, float Value,
		const FTDSkillRow& Row, const TArray<AActor*>& Targets);

	void ClearCastTimers();

	/** 지금 스킬 레벨에서의 마나 소모량. */
	float GetManaCost(const FTDSkillRow& Row, int32 SkillLevel) const;

	/** 쿨다운회복률을 반영한 실제 쿨타임. 평타와 같은 규칙이다. */
	float GetActualCooldown(const FTDSkillRow& Row) const;

	// ── 시전 상태 ─────────────────────────────────────────
	// 복제하지 않는다. Multicast 두 개가 전 머신의 값을 맞춰 준다 —
	// 복제 프로퍼티로 두면 시작·종료 순서가 연출과 어긋날 수 있다.

	FName CastingSkillId;

	/** 첫 판정에서 마나를 치렀는가. 끊겼을 때 쿨을 걸지 말지의 근거다. 서버에서만 쓴다. */
	bool bCostPaid = false;

	/** 시전이 시작된 시각. 이동 감시의 유예를 재는 데 쓴다. */
	float CastStartTime = 0.f;

	FTimerHandle CastTimerHandle;
	FTimerHandle ChannelTickHandle;
	FTimerHandle ChannelEndHandle;

	/**
	 * 스킬별 쿨 종료 시각(각 머신의 월드 시간).
	 *
	 * 복제하지 않고 MulticastOnCastEnded 를 받은 시점에 각자 계산한다. 핑만큼 어긋나지만
	 * 표시용으로는 충분하고, 실제 거부는 언제나 서버가 자기 값으로 한다.
	 */
	TMap<FName, float> CooldownEndTimes;
};
