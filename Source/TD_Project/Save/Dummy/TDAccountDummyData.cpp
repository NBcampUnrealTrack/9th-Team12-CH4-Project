#include "Save/Dummy/TDAccountDummyData.h"
#include "Engine/DataTable.h"
#include "Settings/TDCharacterClassSettings.h"

namespace
{
    TArray<FString> RowOptions(UDataTable* Table)
    {
        TArray<FString> Result;
        if (Table)
            for (FName Name : Table->GetRowNames()) Result.Add(Name.ToString());
        Result.Sort();
        Result.Insert(TEXT("None"), 0);
        return Result;
    }
}

TArray<FString> UTDAccountDummyData::GetClassOptions()
{
    return RowOptions(GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous());
}

TArray<FString> UTDAccountDummyData::GetItemOptions()
{
    return RowOptions(GetDefault<UTDAccountDummySettings>()->ItemTable.LoadSynchronous());
}

TArray<FString> UTDAccountDummyData::GetLocationPresetOptions() const
{
    TArray<FString> Result;
    for (const auto& Preset : LocationPresets)
        if (!Preset.Id.IsNone()) Result.AddUnique(Preset.Id.ToString());
    Result.Sort();
    Result.Insert(TEXT("None"), 0);
    return Result;
}

TArray<FTDDummyAccountRecord> UTDAccountDummyData::BuildInitialAccounts() const
{
    TArray<FTDDummyAccountRecord> Result = Accounts;
    for (auto& Account : Result)
        for (auto& Character : Account.Characters)
        {
            if (Character.InitialLocationPreset.IsNone()) continue;
            const auto* Preset = LocationPresets.FindByPredicate([&](const FTDDummyLocationPreset& Entry)
            { return Entry.Id == Character.InitialLocationPreset; });
            if (Preset && Preset->ZoneId.IsValid() && !Preset->Location.ContainsNaN())
            {
                Character.Data.LastZoneId = Preset->ZoneId;
                Character.Data.LastLocation = Preset->Location;
                Character.Data.bHasSavedLocation = true;
            }
            else UE_LOG(LogTemp, Warning, TEXT("더미 위치 프리셋 '%s'이 없거나 잘못되었습니다. 캐릭터 Data의 위치를 사용합니다."), *Character.InitialLocationPreset.ToString());
        }
    return Result;
}


#if WITH_EDITOR
#include "Misc/DataValidation.h"

void UTDAccountDummyData::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    // 새 계정/캐릭터를 에디터에서 추가하면 빈 ID만 발급한다.
    for (auto& Account : Accounts)
    {
        if (!Account.AccountId.IsValid()) Account.AccountId = FGuid::NewGuid();
        for (auto& Character : Account.Characters)
            if (!Character.CharacterId.IsValid()) Character.CharacterId = FGuid::NewGuid();
    }
    Super::PostEditChangeProperty(Event);
}

EDataValidationResult UTDAccountDummyData::IsDataValid(FDataValidationContext& Context) const
{
    const auto ParentResult = Super::IsDataValid(Context);
    bool bValid = ParentResult != EDataValidationResult::Invalid;
    TSet<FName> PresetIds;
    for (const auto& Preset : LocationPresets)
    {
        if (Preset.Id.IsNone() || PresetIds.Contains(Preset.Id) || !Preset.ZoneId.IsValid() || Preset.Location.ContainsNaN())
        {
            Context.AddError(FText::FromString(TEXT("위치 프리셋의 이름 중복, Zone 또는 좌표를 확인하세요.")));
            bValid = false;
        }
        PresetIds.Add(Preset.Id);
    }
    TSet<FString> Logins;
    TSet<FGuid> AccountIds, CharacterIds;
    for (const auto& Account : Accounts)
    {
        if (Account.LoginId.IsEmpty() || Account.Password.IsEmpty()
            || Account.LoginId.Len() > 64 || Account.Password.Len() > 128
            || Logins.Contains(Account.LoginId) || !Account.AccountId.IsValid()
            || AccountIds.Contains(Account.AccountId)
            || Account.Characters.Num() > UTDAccountSubSystem::MaxCharacters)
        {
            Context.AddError(FText::FromString(TEXT("계정 ID/로그인 중복, 빈 로그인 정보 또는 캐릭터 6개 제한을 확인하세요.")));
            bValid = false;
        }
        Logins.Add(Account.LoginId);
        AccountIds.Add(Account.AccountId);
        for (const auto& Character : Account.Characters)
        {
            if (!Character.CharacterId.IsValid() || CharacterIds.Contains(Character.CharacterId)
                || Character.CharacterName.IsEmpty() || Character.Data.ClassId.IsNone())
            {
                Context.AddError(FText::FromString(TEXT("캐릭터 ID 중복, 이름 또는 직업을 확인하세요.")));
                bValid = false;
            }
            if (!Character.InitialLocationPreset.IsNone() && !PresetIds.Contains(Character.InitialLocationPreset))
            {
                Context.AddError(FText::FromString(TEXT("캐릭터가 선택한 위치 프리셋이 목록에 없습니다.")));
                bValid = false;
            }
            CharacterIds.Add(Character.CharacterId);
        }
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
