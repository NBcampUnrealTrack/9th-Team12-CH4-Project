#pragma once

#include "CoreMinimal.h"
#include "Player/TDPlayerController.h"
#include "TDUI_Login_PlayerController.generated.h"

/** 더미 로그인 테스트 전용. 기본 게임 클래스에는 테스트 기능을 추가하지 않는다. */
UCLASS()
class TD_PROJECT_API ATDUI_Login_PlayerController : public ATDPlayerController
{
    GENERATED_BODY()
public:
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerLogin(const FString& LoginId, const FString& Password);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerSelectCharacter(int32 SlotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerSaveCharacter();
    /** 서버에서 원하는 저장 시점에만 호출한다. 자동 저장은 없다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="TD|UI|Account")
    bool SaveCharacter();
    UFUNCTION(BlueprintPure, Category="TD|UI|Account")
    bool IsLoggedIn() const { return DummyAccountId.IsValid(); }
    UFUNCTION(BlueprintPure, Category="TD|UI|Account")
    FString GetAccountMessage() const { return DummyMessage; }
    UFUNCTION(Client, Reliable)
    void ClientAccountMessage(const FString& Message);
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDLoginScreen();
    UFUNCTION(Exec)
    void TDSave();
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDCharacterSelect();
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDLogout();
    UFUNCTION(Server, Reliable)
    void ServerLeaveCharacter(bool bLogout);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Account")
    TSubclassOf<class UTDLoginWidget> DummyLoginWidgetClass;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY(Replicated) FGuid DummyAccountId;
    FGuid DummyCharacterId;
    FString DummyMessage;

};
