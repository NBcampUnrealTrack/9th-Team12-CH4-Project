#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDStatComponent.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnStatsChanged);

/**
 * 캐릭터의 스탯을 담당하는 컴포넌트.
 *
 * 플레이어와 몬스터가 같은 컴포넌트를 쓴다. 버프·디버프가 양쪽에 동일하게 적용돼야 하므로
 * 액터 상속으로 나누지 않는다. 다만 부착 위치는 다르다 —
 * 플레이어는 APlayerState(리스폰해도 스탯이 유지돼야 함), 몬스터는 Pawn.
 *
 * 이 컴포넌트는 모디파이어를 만들지 않는다. 장비·룬·버프가 만들어 넘긴 것을
 * 소스 단위로 등록받아 모아두고, 조회 시점에 합산할 뿐이다.
 *
 * 소스 등록/제거는 서버에서만 일어난다. 계산 결과는 복제하지 않으며,
 * 클라이언트에는 AttributeSet을 통해 최종값만 전달된다(S5에서 연결).
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDStatComponent();

	// ── 소스 관리 (서버 전용) ─────────────────────────────

	/**
	 * 모디파이어 묶음을 등록하고 핸들을 돌려준다. 장비 1개 = 소스 1개로 잡는다.
	 *
	 * 개별 모디파이어가 아니라 묶음으로 다루는 이유는 제거 때문이다. 반지 두 개가
	 * 똑같이 "화염 저항 +20"을 준다면 값만으로는 어느 쪽을 뺄지 판별할 수 없다.
	 *
	 * @param SourceTag  출처 식별용. 디버깅과 UI 표시에 쓴다(예: Item.Slot.Ring).
	 * @return 서버가 아니거나 등록에 실패하면 무효 핸들.
	 */
	FTDStatSourceHandle AddSource(FGameplayTag SourceTag, TArray<FTDStatModifier> Modifiers);

	/** 소스를 통째로 제거한다. @return 실제로 제거됐으면 true. */
	bool RemoveSource(const FTDStatSourceHandle& Handle);

	/**
	 * Duration 초 뒤에 저절로 걷히는 소스. 버프·디버프용이다.
	 *
	 * AddSource 와 완전히 같은 경로를 쓰고 타이머 하나만 얹는다 — 지속 효과를 위한
	 * 별도 시스템을 만들지 않는 이유가 그것이다. 장비가 스탯을 올리는 방식과
	 * 버프가 올리는 방식이 갈리면 "장비로는 되는데 버프로는 안 되는" 스탯이 생긴다.
	 *
	 * **같은 버프를 다시 걸면 소스가 하나 더 쌓인다.** 중첩을 막지 않는 이유는
	 * 파티원 여럿이 같은 버프를 걸어 주는 경우가 정상이기 때문이다. 겹치면 안 되는
	 * 버프가 생기면 SourceTag 로 기존 것을 찾아 ReplaceSource 를 쓰면 된다.
	 *
	 * @param Duration  0 이하면 아무것도 하지 않는다 — 즉발 효과가 잘못 들어온 것이다.
	 * @return 등록된 소스의 핸들. 시간이 되기 전에 손으로 걷어야 할 때 쓴다.
	 */
	FTDStatSourceHandle AddTimedSource(FGameplayTag SourceTag,
		TArray<FTDStatModifier> Modifiers, float Duration);

	/**
	 * 기존 소스를 걷어내고 새 묶음으로 갈아 끼운다. 갱신이 필요한 곳은 이걸 써야 한다.
	 *
	 * RemoveSource 후 AddSource 를 부르면 결과는 같지만 그 사이에 "모디파이어가 없는 순간"이
	 * 생기고, OnStatsChanged 가 그 중간 상태까지 바깥에 알린다. 레벨업 한 번에 최대 체력이
	 * 400 → 100 → 400 으로 튀는 식이라, 그 찰나에 피해를 입으면 현재 체력이 100 으로 깎인다.
	 * 이 함수는 교체를 한 동작으로 처리하고 알림을 마지막에 한 번만 낸다.
	 *
	 * @param OldHandle  걷어낼 소스. 무효 핸들이면 추가만 한다.
	 * @param Modifiers  비어 있으면 제거만 하고 무효 핸들을 돌려준다.
	 * @return 새로 등록된 소스의 핸들. 호출한 쪽은 이 값을 보관해 두었다가 다음 교체에 넘긴다.
	 */
	FTDStatSourceHandle ReplaceSource(const FTDStatSourceHandle& OldHandle,
		FGameplayTag SourceTag, TArray<FTDStatModifier> Modifiers);

	/** 등록된 소스를 전부 비운다. 리스폰이나 리스펙에 쓴다. */
	void ClearSources();

	// ── 기본값 ────────────────────────────────────────────

	/**
	 * 스탯의 기본값을 설정한다. DT_StatDefinition의 DefaultValue가 여기로 들어온다.
	 * 모디파이어가 붙기 전의 맨 밑바탕이며, Op::Base 모디파이어와 함께 합산된다.
	 */
	void SetBaseValue(FGameplayTag Stat, float Value);

	float GetBaseValue(FGameplayTag Stat) const;

	// ── 조회 ──────────────────────────────────────────────

	/**
	 * 조건 없이 항상 적용되는 모디파이어만 반영한 최종값. 캐릭터 시트에 표시할 값이다.
	 * 결과는 캐시되며 소스가 바뀔 때까지 재계산하지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetStat(FGameplayTag Stat) const;

	/**
	 * 상황이 정해진 시점의 최종값. 조건부 모디파이어까지 평가한다.
	 *
	 * "화염 스킬 데미지 +30%" 같은 옵션은 어떤 스킬을 쓰느냐에 따라 적용 여부가
	 * 달라져서 미리 합쳐둘 수 없다. 스킬 시전처럼 컨텍스트가 확정되는 순간에 호출한다.
	 * 캐시하지 않으므로 매 호출마다 계산한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetStatWithContext(FGameplayTag Stat, const FGameplayTagContainer& Context) const;

	/**
	 * 전투력. 스탯을 종합한 이론상 딜량이다.
	 *
	 *   주공격력 = max(물리, 마법)            ← 전체공격력 모디파이어는 계층 매칭으로 이미 포함
	 *   크리배율 = 1 + 크리율 × (크리댐 - 1)
	 *   보스배율 = 1 + 보스공격력
	 *   전투력   = 주공격력 × 크리배율 × 보스배율
	 *
	 * 방어 계열은 넣지 않는다. 딜과 방어는 단위가 달라 더하면 가중치에 근거가 없어지고,
	 * "전투력은 올랐는데 딜은 그대로"인 상황이 생겨 지표의 의미가 흐려진다.
	 *
	 * 방어력 무시도 빠져 있다. 대상의 방어력을 알아야 효과가 정해지는데
	 * 전투력은 대상이 없는 값이다.
	 *
	 * 몬스터도 같은 컴포넌트를 쓰므로 몬스터 전투력이 함께 나온다.
	 *
	 * **서버에서만 정확한 값이다.** 이 컴포넌트는 복제되지 않으므로 클라이언트에서 부르면
	 * DT_StatDefinition 의 기본값만으로 계산한 값이 나온다.
	 * UI 는 ATDPlayerState::GetCombatPower() 를 써야 한다 — 그쪽이 복제된 값이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetCombatPower() const;

	/**
	 * DT_StatDefinition 에 정의된 스탯 태그 전부.
	 *
	 * 복제용 스냅샷을 만들 때 "무엇을 보낼지"의 목록이 된다. 테이블에 행을 추가하면
	 * 자동으로 따라오므로, 스탯이 늘어도 복제 코드를 고칠 일이 없다.
	 *
	 * 조회된 스탯만 담기는 CachedStats 와 다르다 — 그쪽은 아직 아무도 안 물어본 스탯이 빠져 있다.
	 */
	TArray<FGameplayTag> GetDefinedStats() const;

	/** 소스나 기본값이 바뀌어 스탯이 갱신됐을 때. UI 갱신은 Tick이 아니라 이걸 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Stats")
	FTDOnStatsChanged OnStatsChanged;

protected:
	virtual void BeginPlay() override;

	/**
	 * DT_StatDefinition. 기본값과 허용 범위를 여기서 읽는다.
	 *
	 * 지정하지 않아도 계산은 동작하지만 클램프가 적용되지 않는다. 받는 피해 감소가
	 * 1.0 을 넘어 무적이 되는 것을 막으려면 반드시 지정해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Stats")
	TObjectPtr<UDataTable> StatTable;

private:
	/** DT_StatDefinition 에서 읽은 스탯 하나의 허용 범위. */
	struct FStatRange
	{
		float Min = -TNumericLimits<float>::Max();
		float Max = TNumericLimits<float>::Max();
	};

	/**
	 * 테이블을 읽어 기본값을 채우고 범위를 캐시한다.
	 *
	 * 매 계산마다 테이블을 순회하지 않으려고 한 번만 훑는다. 정적 데이터라 게임 도중
	 * 바뀌지 않으므로 다시 읽을 일이 없다.
	 */
	void LoadStatDefinitions();

	TMap<FGameplayTag, FStatRange> StatRanges;

	/** 등록된 모디파이어 묶음 하나. */
	struct FSource
	{
		FGameplayTag SourceTag;
		TArray<FTDStatModifier> Modifiers;
	};

	float ComputeStat(FGameplayTag Stat, const FGameplayTagContainer& Context) const;

	/** 소스별로 흩어진 모디파이어를 순회용 배열 하나로 펼친다. 소스 변경 시에만 호출된다. */
	void RebuildFlatModifiers();

	/**
	 * 캐시를 통째로 버린다. 알림은 내지 않는다.
	 *
	 * 값을 건드리는 곳마다 불러야 하고 여러 번 불러도 무해하다. 반대로 알림은 변경이
	 * 완결된 순간에 한 번만 나가야 해서, 둘을 한 함수에 묶으면 중간 상태까지 새어 나간다.
	 */
	void InvalidateCache();

	/** 스탯 변경을 구독자에게 알린다. 한 번의 논리적 변경마다 한 번만 부른다. */
	void NotifyStatsChanged();

	bool HasAuthorityToModify() const;

	TMap<FTDStatSourceHandle, FSource> Sources;

	/** AddTimedSource 가 건 타이머들. 걷힐 때 자기 항목을 지우므로 쌓이지 않는다. */
	TMap<FTDStatSourceHandle, FTimerHandle> TimedSourceTimers;

	int32 NextSourceId = 0;

	/** Sources를 펼친 결과. 조회할 때마다 맵을 순회하지 않으려고 유지한다. */
	TArray<FTDStatModifier> FlatModifiers;

	TMap<FGameplayTag, float> BaseValues;

	/**
	 * 조회된 스탯만 담기는 지연 캐시.
	 *
	 * 미리 전부 계산해 둘 수 없다. 태그 계층 매칭 때문에 상위 Damage 모디파이어 하나만
	 * 있어도 Damage.Fire를 조회할 수 있어서, "존재하는 스탯 목록"이 확정되지 않는다.
	 */
	mutable TMap<FGameplayTag, float> CachedStats;

	mutable bool bCacheValid = false;

	/** 전투력도 스탯과 같은 주기로 무효화된다. 장비를 바꿀 때마다 다시 곱할 필요가 없다. */
	mutable float CachedCombatPower = 0.f;

	mutable bool bCombatPowerValid = false;
};
