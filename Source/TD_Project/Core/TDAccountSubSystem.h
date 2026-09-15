#pragma once

#include "CoreMinimal.h"
#include "Save/TDPlayerSaveData.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDAccountSubSystem.generated.h"

class ATDPlayerState;

/** 캐릭터 ID와 계정 소유 관계는 저장 내용 밖에서 관리한다. */
USTRUCT(BlueprintType)
struct FTDDummyCharacterRecord
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FGuid CharacterId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FString CharacterName;
    /** None이면 Data의 존/좌표 설정을 사용한다. 프리셋은 초기 로드에만 적용한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Initial Location", meta=(GetOptions="GetLocationPresetOptions"))
    FName InitialLocationPreset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FTDPlayerSaveData Data;
};

USTRUCT(BlueprintType)
struct FTDDummyAccountRecord
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FGuid AccountId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FString LoginId;
	/** 공개된 개발용 자격 증명. 실제 사용자 비밀번호를 넣지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") FString Password;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Account") TArray<FTDDummyCharacterRecord> Characters;
};

UCLASS()
class TD_PROJECT_API UTDAccountSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxCharacters = 6;
	static constexpr int32 MinPasswordLength = 8;
	static constexpr int32 MaxPasswordLength = 64;
	/** 백엔드 인증 규격과 동일한 로그인 ID 정규화·검증 규칙. */
	static FString NormalizeLoginId(const FString& LoginId);
	static bool IsValidLoginId(const FString& LoginId);
	/** 공백을 제외한 출력 가능한 ASCII 문자만 허용한다. */
	static bool IsValidPassword(const FString& Password, bool bRequireMinimumLength);
	static FString FilterPasswordInput(const FString& Password);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	bool Login(ATDPlayerState* Player, const FString& LoginId, const FString& Password,
		FGuid& OutAccountId, FString& OutError);
	bool RegisterAccount(ATDPlayerState* Player, const FString& LoginId, const FString& Password, FString& OutError);
    bool CreateCharacter(ATDPlayerState* Player, const FString& Name, FName ClassId, FString& OutError);
    bool DeleteCharacter(ATDPlayerState* Player, const FGuid& CharacterId, FString& OutError);
	void Logout(ATDPlayerState* Player);
    /** 서버에서 캐릭터 상태를 새로 만들 때 인증된 세션만 이전한다. */
    bool TransferSession(ATDPlayerState* Previous, ATDPlayerState* Next);
	TArray<FTDCharacterSummary> ListCharacters(const ATDPlayerState* Player) const;
	bool LoadCharacter(const ATDPlayerState* Player, int32 SlotIndex, FTDDummyCharacterRecord& Out) const;
	bool SaveCharacter(const ATDPlayerState* Player, const FGuid& CharacterId, const FTDPlayerSaveData& Data);
private:
	const FTDDummyAccountRecord* FindAccount(const ATDPlayerState* Player) const;
	UPROPERTY() TArray<FTDDummyAccountRecord> Accounts;
	TMap<TWeakObjectPtr<ATDPlayerState>, FGuid> Sessions;
};
