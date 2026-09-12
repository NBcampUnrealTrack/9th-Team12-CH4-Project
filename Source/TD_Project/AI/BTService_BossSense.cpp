#include "AI/BTService_BossSense.h"

#include "AI/TDBossAIController.h"
#include "AI/TDBossAITypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/TDBossCharacter.h"
#include "Character/TDCharacterBase.h"

UBTService_BossSense::UBTService_BossSense()
{
	NodeName = TEXT("Boss Sense");
	Interval = 0.2f;
	RandomDeviation = 0.f;
}

void UBTService_BossSense::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	ATDBossAIController* AIC = Cast<ATDBossAIController>(OwnerComp.GetAIOwner());
	ATDBossCharacter* Boss = AIC ? AIC->GetBoss() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (Boss == nullptr || BB == nullptr || Boss->IsDead())
	{
		return;
	}

	
	if (Boss->IsReturning()) { return; }
	// 1. 리시: 집에서 너무 멀리 끌려 나갔으면 귀환·풀피. 추격전으로 보스를 방 밖으로 끌고 가는 걸 막는다.
	if (Boss->IsFightActive() &&
		FVector::Dist(Boss->GetActorLocation(), Boss->GetHomeLocation()) > Boss->GetLeashRadius())
	{
		Boss->ResetFight();
		BB->ClearValue(TDBossBB::Target);
		BB->SetValueAsBool(TDBossBB::bFightActive, false);
		BB->SetValueAsBool(TDBossBB::bBusy, Boss->IsBusy());
		return;
	}

	// 2. 대상: 지금 대상이 살아 있고 범위 안이면 유지(어그로 안정), 아니면 가장 가까운 적.
	//    전투 전엔 EngageRadius, 전투 중엔 LeashRadius 가 탐색 범위다.
	const float SearchRadius = Boss->IsFightActive() ? Boss->GetLeashRadius() : AIC->GetEngageRadius();
	ATDCharacterBase* Target = Cast<ATDCharacterBase>(BB->GetValueAsObject(TDBossBB::Target));

	const bool bTargetValid = Target != nullptr && !Target->IsDead() &&
		FVector::Dist(Boss->GetActorLocation(), Target->GetActorLocation()) <= SearchRadius;
	if (!bTargetValid)
	{
		Target = Boss->FindEnemy(false, SearchRadius);
	}

	// 3. 전투 시작·종료
	if (Target != nullptr && !Boss->IsFightActive())
	{
		Boss->BeginFight(Target);   // 입장 연출. 끝날 때까지 IsBusy
	}
	else if (Target == nullptr && Boss->IsFightActive())
	{
		Boss->ResetFight();         // 전원 사망·이탈 → 귀환
	}

	// 4. 기록. 거리는 XY 중심 거리 — 패턴 스펙의 Min/MaxDistance 와 같은 자.
	Boss->SetPatternTarget(Target);
	if (Target != nullptr)
	{
		BB->SetValueAsObject(TDBossBB::Target, Target);
		BB->SetValueAsFloat(TDBossBB::Distance,
			FVector::Dist2D(Boss->GetActorLocation(), Target->GetActorLocation()));
	}
	else
	{
		BB->ClearValue(TDBossBB::Target);
		BB->SetValueAsFloat(TDBossBB::Distance, 0.f);
	}
	BB->SetValueAsInt(TDBossBB::Phase, Boss->GetPhase());
	BB->SetValueAsBool(TDBossBB::bBusy, Boss->IsBusy());
	BB->SetValueAsBool(TDBossBB::bFightActive, Boss->IsFightActive());
}