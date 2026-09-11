#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"
#include "BTTask_BossExecutePattern.generated.h"

class ATDBossCharacter;
class UBehaviorTreeComponent;

/** 이 태스크의 보스별 상태. 노드 객체는 트리마다 하나라 여기에 둬야 보스 둘이 섞이지 않는다. */
struct FBTBossExecutePatternMemory
{
	TWeakObjectPtr<ATDBossCharacter> Boss;
	FDelegateHandle FinishedHandle;
	int32 PatternIndex = INDEX_NONE;
};

/**
 * NextPattern 을 실행하고 선딜→타격→후딜이 끝날 때까지 InProgress 로 기다린다.
 * 끝은 보스의 OnPatternFinished(서버 네이티브 델리게이트)가 알려준다 — 폴링하지 않는다.
 * 잠수 패턴은 가장 먼 적을 노린다(원거리 견제).
 */
UCLASS()
class TD_PROJECT_API UBTTask_BossExecutePattern : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BossExecutePattern();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTBossExecutePatternMemory); }

private:
	void HandlePatternFinished(int32 PatternIndex, TWeakObjectPtr<UBehaviorTreeComponent> OwnerComp);
	void Unbind(FBTBossExecutePatternMemory* Memory);
};