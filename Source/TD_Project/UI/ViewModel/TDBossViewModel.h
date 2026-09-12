#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "TDBossViewModel.generated.h"

class ATDBossCharacter;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnBossDisplayChanged);

/** 보스 한 마리의 표시 데이터와 구독만 소유한다. UI 생성/입력/대상 선택은 하지 않는다. */
UCLASS(BlueprintType, meta=(MVVMAllowedContextCreationType="Manual"))
class TD_PROJECT_API UTDBossViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** nullptr이면 구독 및 표시 데이터를 모두 초기화한다. 같은 대상 재설정은 중복 구독하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss")
		void SetSource(ATDBossCharacter* InBoss);

	UFUNCTION(BlueprintPure, Category="TD|UI|Boss")
		ATDBossCharacter* GetSource() const;

	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss")
		void RefreshAll();

	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		FText BossName;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		int32 BossLevel = 0;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		float HealthPercent = 0.f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		bool bHasBoss = false;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|UI|Boss")
		bool bIsDead = false;

	/** 전체 필드 갱신이 끝난 뒤 발생. 단순 C++/BP 위젯 연결에 사용한다. */
	UPROPERTY(BlueprintAssignable, Category="TD|UI|Boss")
		FTDOnBossDisplayChanged OnDisplayChanged;

private:
	TWeakObjectPtr<ATDBossCharacter> Source;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle IdentityHandle;
	void UnbindSource();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	UFUNCTION()
		void HandleDeathStateChanged();
	UFUNCTION()
		void HandleSourceEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
	UFUNCTION()
		void HandleSourceDestroyed(AActor* Actor);
};
