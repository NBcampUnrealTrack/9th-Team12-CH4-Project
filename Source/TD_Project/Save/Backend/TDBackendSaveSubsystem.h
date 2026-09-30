#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Save/TDPlayerSaveData.h"
#include "TDBackendSaveSubsystem.generated.h"

class ATDPlayerController;
struct FTDBackendSession;

/** Server-only HTTP account storage. Credentials never replicate or enter config assets. */
UCLASS()
class TD_PROJECT_API UTDBackendSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;
    bool IsEnabled() const
    {
        return !BaseUrl.IsEmpty();
    }
    void Login(ATDPlayerController *PC, const FString &Id, const FString &Password, bool bRegister = false);
    void Select(ATDPlayerController *PC, int32 ListIndex);
    void Create(ATDPlayerController *PC, const FString &Name, FName ClassId);
    void Delete(ATDPlayerController *PC, const FGuid &CharacterId);
    bool Save(ATDPlayerController *PC);
    void Leave(ATDPlayerController *PC, bool bLogout);
    void Disconnect(ATDPlayerController *PC);

  private:
    friend class FTDBackendHttpTest;
    friend class FTDBackendControllerTest;
    using FReply = TFunction<void(int32, const TSharedPtr<FJsonObject> &)>;
    FString BaseUrl, ServerKey;
    bool bStopping = false;
    TMap<TWeakObjectPtr<ATDPlayerController>, TSharedPtr<FTDBackendSession>> Sessions;
    FTSTicker::FDelegateHandle TickHandle;
#if WITH_EDITOR
    FDelegateHandle PrePIEEndedHandle;
    void OnPrePIEEnded(bool bSimulating);
#endif
    TSharedPtr<FTDBackendSession> Session(ATDPlayerController *PC);
    void Request(const TSharedPtr<FTDBackendSession> &S, const FString &Method, const FString &Path,
                 const FString &Body, FReply Reply, int32 Retries = 0);
    void Refresh(const TSharedPtr<FTDBackendSession> &S, FName Action = NAME_None);
    bool Capture(const TSharedPtr<FTDBackendSession> &S, FString &Out);
    void StartSave(const TSharedPtr<FTDBackendSession> &S);
    void FinishLeave(const TSharedPtr<FTDBackendSession> &S);
    void Report(const TSharedPtr<FTDBackendSession> &S, const FString &Message);
    bool WriteRecovery(const TSharedPtr<FTDBackendSession> &S);
    void Quarantine(const TSharedPtr<FTDBackendSession> &S);
    void RecoverBeforeLoad(const TSharedPtr<FTDBackendSession> &S, TFunction<void()> Continue);
    bool Tick(float Delta);
};
