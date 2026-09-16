#include "Character/TDSilhouetteComponent.h"

#include "Camera/CameraComponent.h"
#include "Character/TDCharacterBase.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbookComponent.h"
#include "Settings/TDCharacterClassSettings.h"

void UTDSilhouetteComponent::EnableForLocalPlayer()
{
	const ATDCharacterBase* Character = Cast<ATDCharacterBase>(GetOwner());
	if (Character == nullptr || !Character->IsLocallyControlled())
	{
		return;
	}

	UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
	UPaperFlipbookComponent* Sprite = Character->GetSpriteComponent();
	if (Camera == nullptr || Sprite == nullptr || BoundCamera.Get() == Camera)
	{
		return;
	}

	UMaterialInterface* Material = UTDCharacterClassSettings::Get()->SilhouetteMaterial.LoadSynchronous();
	if (Material == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("실루엣: 프로젝트 세팅 TD > Character 의 SilhouetteMaterial 이 비어 있어 가려진 캐릭터를 표시하지 않는다."));
		return;
	}

	Sprite->SetRenderCustomDepth(true);

	// 완전히 가려지면 오클루전 컬링이 스프라이트를 렌더링에서 빼 버리고, 커스텀 뎁스도
	// 함께 빠져 실루엣이 끊긴다. 판정에 쓰는 바운딩 박스를 부풀려 살아남게 한다.
	Sprite->SetBoundsScale(OccludedBoundsScale);

	Camera->PostProcessSettings.AddBlendable(Material, 1.f);

	AppliedMaterial = Material;
	BoundCamera = Camera;
}
