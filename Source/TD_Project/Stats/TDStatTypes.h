#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"	// FGameplayTag, FGameplayTagQuery 둘 다 여기에 있다
#include "TDStatTypes.generated.h"

/**
 * 모디파이어 연산 종류.
 *
 * 최종값 = (ΣBase + ΣAdded) × (1 + ΣIncreased) × Π(1 + More)
 *
 * 스탯을 바꾸는 모든 것 — 장비, 룬, 버프, 강화 — 은 이 연산 중 하나로 표현된다.
 * 스탯마다 필드를 늘리는 대신 모디파이어 배열 하나로 통일하기 위한 것.
 */
UENUM(BlueprintType)
enum class ETDModOp : uint8
{
	/**
	 * 스탯의 원천값. 무기 자체의 공격력처럼 "그 스탯이 존재하는 이유".
	 * 계산상으로는 Added와 함께 합산되지만, 나중에 "무기 기본 공격력의 N%"처럼
	 * 원천값만 참조하는 옵션이 나올 수 있으므로 분리해 둔다.
	 */
	Base		UMETA(DisplayName = "Base"),

	/** 원천값에 얹히는 가산 옵션. 장비 접사, 룬, 버프 대부분이 여기 해당한다. */
	Added		UMETA(DisplayName = "Added"),

	/** 합연산 %. 전부 더해진 뒤 한 번만 곱해지므로 여러 개 쌓여도 완만하다. 흔하게 써도 된다. */
	Increased	UMETA(DisplayName = "Increased"),

	/** 곱연산 %. 각각 따로 곱해져 기하급수적으로 커진다. 희귀 옵션에만 쓸 것. */
	More		UMETA(DisplayName = "More")
};

/**
 * 스탯을 변경하는 최소 단위. 모든 시스템이 주고받는 공통 화폐.
 *
 * 장비 접사도, 룬 노드도, 버프도, 스킬 파라미터 수정도 전부 이 구조체로 표현된다.
 * 소비처는 Stat 태그의 네임스페이스로 갈린다 —
 * Stat.* 은 UTDStatComponent가, Skill.Param.* 은 스킬 시전 컨텍스트가 가져간다.
 */
USTRUCT(BlueprintType)
struct FTDStatModifier
{
	GENERATED_BODY()

	/** 대상 스탯 태그. 예: Stat.Offense.Damage , Skill.Param.Projectile.Count */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Modifier")
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Modifier")
	ETDModOp Op = ETDModOp::Added;

	/** Increased/More는 비율값. 20% 증가는 0.2로 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Modifier")
	float Value = 0.f;

	/**
	 * 발동 조건. 비어 있으면 무조건부다.
	 *
	 * 무조건부 모디파이어만 캐릭터 최종 스탯으로 캐시할 수 있다.
	 * 조건부("화염 스킬 데미지 +30%")는 어떤 스킬을 쓰느냐에 따라 적용 여부가
	 * 달라지므로 미리 합쳐둘 수 없고, 시전 시점에 컨텍스트 태그로 평가해야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Modifier")
	FGameplayTagQuery Condition;

	FTDStatModifier() = default;

	FTDStatModifier(FGameplayTag InStat, ETDModOp InOp, float InValue)
		: Stat(InStat), Op(InOp), Value(InValue)
	{
	}

	bool IsConditional() const { return !Condition.IsEmpty(); }
};

/**
 * 계산이 끝난 스탯 하나. 클라이언트에 결과만 보내기 위한 것이다.
 *
 * UTDStatComponent 는 복제되지 않는다 — 모디파이어 원본을 보내면 계산 규칙까지
 * 클라이언트가 알아야 하고, 그러면 서버와 어긋날 여지가 생긴다.
 * 대신 서버가 계산한 최종값만 이 구조체로 실어 보낸다.
 *
 * 태그를 값과 함께 보내는 이유는 순서에 의존하지 않기 위해서다. 인덱스로만 보내면
 * 스탯 목록의 순서가 바뀌는 순간 화면에 엉뚱한 값이 조용히 표시된다.
 */
USTRUCT(BlueprintType)
struct FTDStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stat")
	FGameplayTag Stat;

	UPROPERTY(BlueprintReadOnly, Category = "Stat")
	float Value = 0.f;

	FTDStatSnapshot() = default;

	FTDStatSnapshot(FGameplayTag InStat, float InValue)
		: Stat(InStat), Value(InValue)
	{
	}

	bool operator==(const FTDStatSnapshot& Other) const
	{
		return Stat == Other.Stat && Value == Other.Value;
	}
};

/**
 * 모디파이어 묶음 하나를 가리키는 핸들.
 *
 * 개별 모디파이어가 아니라 소스 단위로 등록/제거하기 위한 것.
 * 반지 두 개가 똑같이 "화염 저항 +20"을 준다면 값만으로는 어느 쪽을 뺄지
 * 판별할 수 없으므로, 장비 1개 = 소스 1개로 묶어 핸들로 식별한다.
 *
 * 서버에서만 발급되며 복제하지 않는다. 클라이언트는 계산된 결과만 받는다.
 */
USTRUCT(BlueprintType)
struct FTDStatSourceHandle
{
	GENERATED_BODY()

	FTDStatSourceHandle() = default;
	explicit FTDStatSourceHandle(int32 InId) : Id(InId) {}

	bool IsValid() const { return Id != INDEX_NONE; }
	void Invalidate() { Id = INDEX_NONE; }

	bool operator==(const FTDStatSourceHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FTDStatSourceHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FTDStatSourceHandle& Handle)
	{
		return ::GetTypeHash(Handle.Id);
	}

private:
	UPROPERTY()
	int32 Id = INDEX_NONE;
};
