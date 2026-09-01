#include "World/TDPortal.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/TDPlayerController.h"

ATDPortal::ATDPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 150.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 레벨 배치 액터라 클라이언트도 이미 갖고 있다. 겹침 판정은 서버에서만 하고,
	// 결과인 좌표 이동은 캐릭터 이동 복제를 타고 전달된다.
	bReplicates = false;
}

void ATDPortal::BeginPlay()
{
	Super::BeginPlay();

	// 클라이언트에서도 겹침이 발생하지만 거기서는 아무 일도 하지 않는다.
	// 아예 구독하지 않아 불필요한 호출 자체를 없앤다.
	if (!HasAuthority())
	{
		return;
	}

	if (!TargetZoneId.IsValid())
	{
		// 조용히 두면 "밟아도 아무 일이 없다" 는 증상만 남는다(§11-D).
		UE_LOG(LogTemp, Warning,
			TEXT("포탈 '%s' 에 TargetZoneId 가 지정되지 않았다. 이 포탈은 동작하지 않는다."),
			*GetName());
	}

	// 상호작용형은 밟는 것만으로 발동하지 않으므로 겹침을 구독할 필요가 없다.
	// 입력 처리가 붙으면 그쪽에서 Activate 를 부른다.
	if (!bRequiresInteraction)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATDPortal::HandleBeginOverlap);
	}
}

void ATDPortal::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	const APawn* OverlappedPawn = Cast<APawn>(OtherActor);
	if (OverlappedPawn == nullptr)
	{
		return;
	}

	// 몬스터가 밟아도 반응하지 않는다. 플레이어가 조종하는 Pawn 만 통과시킨다.
	TryTravel(Cast<APlayerController>(OverlappedPawn->GetController()));
}

void ATDPortal::Activate(APlayerController* Player)
{
	// 상호작용형이든 아니든 같은 경로를 쓴다. 블루프린트나 입력에서 직접 부를 수 있도록 열어둔다.
	if (HasAuthority())
	{
		TryTravel(Player);
	}
}

void ATDPortal::TryTravel(APlayerController* Player)
{
	if (!HasAuthority() || Player == nullptr || !TargetZoneId.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 쿨다운. 도착 지점 근처에 반대편 포탈이 있으면 이것이 없을 때 무한히 왕복한다.
	const double Now = World->GetTimeSeconds();

	if (const double* LastUse = LastUseTimes.Find(Player))
	{
		if (Now - *LastUse < Cooldown)
		{
			return;
		}
	}

	const ATDGameMode* ConstGameMode = World->GetAuthGameMode<ATDGameMode>();
	ATDGameMode* GameMode = const_cast<ATDGameMode*>(ConstGameMode);
	if (GameMode == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("포탈 '%s': GameMode 를 찾지 못했다."), *GetName());
		return;
	}

	// 판정은 전부 GameMode 가 한다. 포탈은 요청만 넘긴다.
	const ETDZoneTravelResult Result =
		GameMode->RequestZoneTravel(Player, TargetZoneId, EntryName, TargetEntryPoint);

	// 성공했을 때만 쿨다운을 건다. 레벨이 모자라 거부된 경우까지 막으면
	// 레벨업 직후에 다시 시도하려 할 때 기다려야 한다.
	if (Result == ETDZoneTravelResult::Success)
	{
		LastUseTimes.Add(Player, Now);

		// 접속이 끊긴 컨트롤러가 쌓이지 않도록 만료된 항목을 함께 걷어낸다.
		// 포탈 하나가 드나드는 인원만큼만 들고 있으면 된다.
		for (auto It = LastUseTimes.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	// 거부됐으면 그 플레이어에게만 알린다. 화면에 아무 일도 일어나지 않으면
	// 플레이어는 "포탈이 고장났다" 고 받아들인다.
	if (Result != ETDZoneTravelResult::Success)
	{
		if (ATDPlayerController* TDPlayer = Cast<ATDPlayerController>(Player))
		{
			TDPlayer->ClientZoneTravelFailed(TargetZoneId, Result);
		}
	}
}
