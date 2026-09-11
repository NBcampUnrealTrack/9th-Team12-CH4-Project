#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "TDCharacterBase.generated.h"

class UAbilitySystemComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UTDProgressionComponent;
class UTDStatComponent;
class UTDCombatComponent;

/** 캐릭터가 사망했을 때. AI 가 행동을 멈추거나 보상을 지급할 지점이다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnCharacterDeath);

/**
 * 되살아났을 때. 사망 UI 를 닫는 신호다.
 *
 * 사망과 짝이라 같은 곳에서 브로드캐스트한다. 둘을 하나의 델리게이트에
 * bool 로 합치지 않는 이유는 구독하는 쪽이 대개 한쪽만 필요해서다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnCharacterRespawn);

/**
 * 피격 알림(타깃 측). 피격 모션·경직 연출·피격 SFX 가 구독한다. 전 머신에서 불린다.
 * 공격자 측 OnHit(CombatComponent)과 짝이다 — 그쪽은 "내가 때렸다", 이쪽은 "내가 맞았다".
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTDOnCharacterDamaged,
	AActor*, Attacker, float, Damage, bool, bCritical);

/** 서버 전용 피격 알림. AI(어그로)처럼 C++ 만 듣는 구독자용 — 네트워크를 타지 않는다. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FTDOnCharacterDamagedServer,
	AActor* /*Attacker*/, float /*Damage*/, bool /*bCritical*/);

/**
 * 플레이어와 몬스터의 공통 베이스.
 *
 * 스탯 컴포넌트를 직접 소유하지 않는다. 부착 위치가 자식마다 다르기 때문이다 —
 * 몬스터는 Pawn 에 붙지만, 플레이어는 사망 시 Pawn 이 파괴돼도 스탯·버프가 남아야 하므로
 * APlayerState 에 붙는다. 이 클래스는 경로만 제공한다.
 */
UCLASS(Abstract)
class TD_PROJECT_API ATDCharacterBase : public ACharacter, public IGenericTeamAgentInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	
	ATDCharacterBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	UTDCombatComponent* GetCombatComponent() const { return CombatComponent; }

	// ── 2D 표현 ───────────────────────────────────────────
	// 스프라이트를 그리는 것과 애니메이션을 고르는 것이 분리돼 있다.
	// ACharacter 가 갖고 있는 Mesh(SkeletalMesh)는 쓰지 않는다 — 지우면
	// ACharacter 의 기능이 깨지므로 그냥 비워둔다.

	UFUNCTION(BlueprintPure, Category = "TD|Visual")
	UPaperFlipbookComponent* GetSpriteComponent() const { return SpriteComponent; }

	UFUNCTION(BlueprintPure, Category = "TD|Visual")
	UPaperZDAnimationComponent* GetAnimationComponent() const { return AnimationComponent; }
	
	/**
	 * IAbilitySystemInterface.
	 *
	 * 스탯 컴포넌트와 마찬가지로 위치가 자식마다 다르다 —
	 * 플레이어는 PlayerState, 몬스터는 자기 자신. 기본 구현은 nullptr 을 돌려준다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ── IGenericTeamAgentInterface ────────────────────────
	// AIPerception 이 적·아군을 가르는 기준이다. 이게 없으면 AI 쪽에서 클래스를 일일이
	// 캐스팅해 판별해야 하고, 나중에 중립 몬스터나 소환수가 생기면 그 코드를 전부 고쳐야 한다.

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;

	// ── 피격 ──────────────────────────────────────────────

	/**
	 * 서버가 데미지 적용 직후 부른다(CombatStatics::ApplyDamage). 죽인 타격에는 불리지 않는다.
	 * 경직을 걸고, 서버 구독자(AI)에게 알리고, 전 머신에 피격 연출을 방송한다.
	 */
	void ReceiveHit(AActor* Attacker, float Damage, bool bCritical);

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnCharacterDamaged OnDamaged;

	/** C++ 전용(비-다이나믹). AI 컨트롤러가 OnPossess 에서 AddUObject 로 구독한다. */
	FTDOnCharacterDamagedServer OnDamagedServer;

	/** 경직 중인가. 경직 중엔 공격 불가(CanAttack). 서버 값이다 — 클라에선 항상 false. */
	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	bool IsStaggered() const;

	/**
	 * 무적을 Duration 초 동안 건다. 이미 걸려 있으면 **더 긴 쪽이 남는다.**
	 *
	 * 짧은 무적이 긴 무적을 덮어써 끊기면 "방벽을 썼는데 오히려 빨리 풀리는" 상황이 된다.
	 *
	 * 경직(StaggerEndTime)과 같은 방식이다. 스탯으로 두지 않은 이유는
	 * Stat.Defense.DamageReduction 에 1.0 미만 상한이 걸려 있어서다 — 그 상한은
	 * 무적 버그를 막는 옳은 설정이라 100% 를 표현할 수 없다.
	 */
	void SetInvulnerable(float Duration);

	/** 무적인가. ApplyDamage 가 확인한다. 서버 값이다 — 클라에선 항상 false. */
	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	bool IsInvulnerable() const;

	// ── 사망 ──────────────────────────────────────────────

	/**
	 * 사망 알림. **서버·클라이언트 양쪽에서 불린다.**
	 *
	 * 서버는 HandleDeath 안에서, 클라이언트는 bIsDead 가 복제될 때 OnRep 이 부른다.
	 * UI 는 이걸 구독해 사망 화면을 띄우면 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnCharacterDeath OnDeath;

	/** 되살아났을 때. 사망 화면을 닫는 신호다. 역시 양쪽에서 불린다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnCharacterRespawn OnRespawn;

	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	bool IsDead() const { return bIsDead; }

	/**
	 * 사망 처리. 체력이 0 이 됐을 때 호출한다.
	 * 여러 번 불려도 한 번만 동작하므로 호출부에서 중복을 걱정하지 않아도 된다.
	 */
	virtual void HandleDeath();

	/**
	 * 되살리기. **위치와 체력은 건드리지 않는다** — 사망 상태만 푼다.
	 *
	 * 체력 회복과 부활 지점 이동은 게임 규칙이라 `ATDGameMode::RespawnPlayer` 가 맡는다.
	 * 여기서 함께 하면 몬스터를 되살릴 때도 플레이어 규칙이 딸려온다.
	 */
	virtual void HandleRespawn();
	
	
	/**
	 * 이 캐릭터의 스탯 컴포넌트. 없으면 nullptr 가 발생한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	virtual UTDStatComponent* GetStatComponent() const;

	/**
	 * 이 캐릭터의 성장 컴포넌트. 없으면 nullptr 가 발생한다.
	 * 스탯 컴포넌트와 마찬가지로 부착 위치가 자식마다 다르다. 플레이어는 PlayerState 에 있고,
	 * 몬스터는 성장이라는 개념이 없어 애초에 갖지 않는다 — 몬스터에게 nullptr 은 정상이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	virtual UTDProgressionComponent* GetProgressionComponent() const;

	/**
	 * 스탯 조회 단축 경로. 컴포넌트가 아직 없으면 DefaultValue 를 돌려준다.
	 * 호출부마다 null 검사를 반복하지 않으려고 여기 모아둔다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetStat(FGameplayTag Stat, float DefaultValue = 0.f) const;

protected:
	virtual void BeginPlay() override;

	/**
	 * 스탯이 바뀌었을 때 캐릭터에 실제로 반영한다. 지금은 이동 속도만 다룬다.
	 *
	 * 스탯 컴포넌트의 OnStatsChanged 에 연결되며, Tick 으로 감시하지 않는다.
	 */
	UFUNCTION()
	virtual void HandleStatsChanged();

	/** 스탯 컴포넌트가 준비되면 변경 알림을 구독한다. 이미 구독했으면 아무 일도 하지 않는다. */
	void BindToStatComponent();

	/**
	 * 재생 주기(초). Stat.Resource.*.Regen 은 **초당** 값이라 이 간격만큼 곱해 적용된다.
	 *
	 * 1초보다 촘촘하게 둘 이유가 없다. 회복량은 간격에 비례해 보정되므로 총량은 같고,
	 * 짧게 잡으면 어트리뷰트 복제만 늘어난다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Stats", meta = (ClampMin = "0.1"))
	float RegenIntervalSeconds = 1.f;

	/**
	 * 재생 한 번. 서버 타이머가 부른다.
	 *
	 * 전투 중에도 돈다. 멈추려면 "전투 중" 을 판정할 기준이 필요한데, 마지막 피격
	 * 시각이든 어그로든 지금은 그 개념이 없다. 필요해지면 여기에 조건 하나를 더한다.
	 */
	void TickRegen();

	/**
	 * 소속 팀. 값 배정(플레이어 0 / 몬스터 1 등)은 AI 담당과 맞춰야 하므로
	 * 코드에 박지 않고 블루프린트에서 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Team")
	FGenericTeamId TeamId;
	
	/** 피격 시 경직 시간(초). 0 이면 경직 없음. 플레이어는 0, 몬스터는 생성자/BP 에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat", meta = (ClampMin = "0"))
	float HitStaggerDuration = 0.f;

	/** "맞았다" 방송. 서버가 ReceiveHit 에서 쏘고 전원이 OnDamaged 로 받는다. 반복 연출이라 Unreliable. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnDamaged(AActor* Attacker, float Damage, bool bCritical);

private:
	/** 중복 구독을 막기 위한 표시. 플레이어는 PlayerState 복제 시점이 일정하지 않아 여러 번 시도된다. */
	bool bBoundToStatComponent = false;

	/** 재생 타이머. 서버에서만 돈다 — 어트리뷰트를 바꾸는 것은 서버 권한이다. */
	FTimerHandle RegenTimerHandle;

	UFUNCTION()
	void OnRep_IsDead();

	/**
	 * 서버가 정하고 전원에게 복제한다.
	 *
	 * 소유자 전용이 아닌 이유는 남의 사망도 보여야 하기 때문이다 —
	 * 파티원의 상태 표시, 시체에 공격이 안 들어가는 판정, 사망 애니메이션 전부
	 * 다른 클라이언트에서도 알아야 한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;
	
	/** 경직이 끝나는 서버 월드시간. 판정(공격 금지·AI 정지)은 서버 일이라 복제하지 않는다. */
	float StaggerEndTime = -1.f;

	/** 무적이 끝나는 서버 월드시간. 위와 같은 이유로 복제하지 않는다 — 연출은 스킬 시전 방송이 맡는다. */
	float InvulnerableEndTime = -1.f;
	
	/** 플레이어와 몬스터가 같은 공격 경로를 쓴다. 쿨타임·히트박스는 아바타 소유라 Pawn 에 둔다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Combat")
	TObjectPtr<UTDCombatComponent> CombatComponent;

	/** 실제로 화면에 그려지는 스프라이트. PaperZD 가 이 컴포넌트의 플립북을 갈아 끼운다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> SpriteComponent;

	/**
	 * 어떤 애니메이션을 재생할지 정하는 상태 머신.
	 *
	 * AnimInstanceClass 와 RenderComponentRef 는 플러그인에서 private 이라
	 * C++ 로 지정할 수 없다. **블루프린트에서 연결해야 한다** —
	 * 직업별 교체는 나중에 SetAnimInstanceClass() 로 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperZDAnimationComponent> AnimationComponent;
};
