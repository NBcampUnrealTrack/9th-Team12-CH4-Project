#pragma once

#include "CoreMinimal.h"
#include "Save/TDPlayerSaveData.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDAccountSubSystem.generated.h"

class ATDPlayerState;

/** 캐릭터 ID와 계정 소유 관계는 저장 내용 밖에서 관리한다. */
USTRUCT()
struct FTDDummyCharacterRecord
{
	GENERATED_BODY()
	UPROPERTY() FGuid CharacterId;
	UPROPERTY() FString CharacterName;
	UPROPERTY() FTDPlayerSaveData Data;
};

USTRUCT()
struct FTDDummyAccountRecord
{
	GENERATED_BODY()
	UPROPERTY() FGuid AccountId;
	UPROPERTY() FString LoginId;
	/** 공개된 개발용 자격 증명. 실제 사용자 비밀번호를 넣지 않는다. */
	UPROPERTY() FString Password;
	UPROPERTY() TArray<FTDDummyCharacterRecord> Characters;
};

UCLASS()
class TD_PROJECT_API UTDAccountSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxCharacters = 6;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	bool Login(ATDPlayerState* Player, const FString& LoginId, const FString& Password,
		FGuid& OutAccountId, FString& OutError);
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
