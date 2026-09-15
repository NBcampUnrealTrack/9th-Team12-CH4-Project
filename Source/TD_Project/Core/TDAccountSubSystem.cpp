#include "Core/TDAccountSubSystem.h"
#include "Player/TDPlayerState.h"
#include "Save/Dummy/TDAccountDummyData.h"
#include "Data/TDCharacterClassRow.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Engine/DataTable.h"

void UTDAccountSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    const UTDAccountDummyData* Initial = GetDefault<UTDAccountDummySettings>()->InitialAccounts.LoadSynchronous();
    if (Initial) Accounts = Initial->BuildInitialAccounts();
    else UE_LOG(LogTemp, Warning, TEXT("더미 계정 DA가 없습니다. Project Settings > Game > TD Dummy Accounts에서 Initial Accounts를 지정하세요."));
#endif
}

bool UTDAccountSubSystem::Login(ATDPlayerState* Player, const FString& LoginId,
	const FString& Password, FGuid& OutAccountId, FString& OutError)
{
	OutAccountId.Invalidate();
	OutError = TEXT("아이디 또는 비밀번호가 올바르지 않습니다.");
	if (!IsValid(Player) || !Player->HasAuthority() || Player->HasSelectedCharacter()
		|| LoginId.Len() > 64 || Password.Len() > 128) return false;
	const FTDDummyAccountRecord* Account = Accounts.FindByPredicate([&](const FTDDummyAccountRecord& Entry)
	{
		return Entry.LoginId == LoginId && Entry.Password == Password;
	});
	if (!Account) return false;
	for (auto It = Sessions.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); continue; }
		if (It.Value() == Account->AccountId && It.Key().Get() != Player)
		{
			OutError = TEXT("이미 접속 중인 계정입니다. 다른 테스트 계정을 사용하세요.");
			return false;
		}
	}
	Sessions.Add(Player, Account->AccountId);
	OutAccountId = Account->AccountId;
	OutError.Empty();
	return true;
}

void UTDAccountSubSystem::Logout(ATDPlayerState* Player)
{
	Sessions.Remove(Player);
}

const FTDDummyAccountRecord* UTDAccountSubSystem::FindAccount(const ATDPlayerState* Player) const
{
	if (!IsValid(Player) || !Player->HasAuthority()) return nullptr;
	const FGuid* Id = Sessions.Find(TWeakObjectPtr<ATDPlayerState>(const_cast<ATDPlayerState*>(Player)));
	return Id ? Accounts.FindByPredicate([&](const FTDDummyAccountRecord& Entry) { return Entry.AccountId == *Id; }) : nullptr;
}

TArray<FTDCharacterSummary> UTDAccountSubSystem::ListCharacters(const ATDPlayerState* Player) const
{
	TArray<FTDCharacterSummary> Result;
	if (const FTDDummyAccountRecord* Account = FindAccount(Player))
	{
		for (int32 Slot = 0; Slot < FMath::Min(Account->Characters.Num(), MaxCharacters); ++Slot)
		{
			const FTDDummyCharacterRecord& Character = Account->Characters[Slot];
			FTDCharacterSummary& Summary = Result.AddDefaulted_GetRef();
			Summary.CharacterId = Character.CharacterId;
			Summary.CharacterName = Character.CharacterName;
			Summary.ClassId = Character.Data.ClassId;
			Summary.Level = Character.Data.Level;
			for (const FTDItemInstance& Item : Character.Data.EquippedItems) Summary.EquippedItemIds.Add(Item.ItemId);
		}
	}
	return Result;
}

bool UTDAccountSubSystem::LoadCharacter(const ATDPlayerState* Player, int32 SlotIndex, FTDDummyCharacterRecord& Out) const
{
	const FTDDummyAccountRecord* Account = FindAccount(Player);
	if (!Account || SlotIndex < 0 || SlotIndex >= MaxCharacters || !Account->Characters.IsValidIndex(SlotIndex)) return false;
	Out = Account->Characters[SlotIndex];
	return true;
}

bool UTDAccountSubSystem::SaveCharacter(const ATDPlayerState* Player, const FGuid& CharacterId, const FTDPlayerSaveData& Data)
{
	const FTDDummyAccountRecord* Account = FindAccount(Player);
	if (!Account || !CharacterId.IsValid()) return false;
	// 계정 소유 관계를 먼저 검증하므로 다른 계정 캐릭터에는 쓸 수 없다.
	FTDDummyAccountRecord* MutableAccount = Accounts.FindByPredicate([&](const FTDDummyAccountRecord& Entry) { return Entry.AccountId == Account->AccountId; });
	FTDDummyCharacterRecord* Character = MutableAccount->Characters.FindByPredicate([&](const FTDDummyCharacterRecord& Entry) { return Entry.CharacterId == CharacterId; });
	if (!Character) return false;
	Character->Data = Data;
	return true;
}

bool UTDAccountSubSystem::TransferSession(ATDPlayerState* Previous, ATDPlayerState* Next)
{
    if (!FindAccount(Previous) || !IsValid(Next) || !Next->HasAuthority()
        || Previous == Next || Previous->GetOwner() != Next->GetOwner() || Sessions.Contains(Next)) return false;
    const FGuid AccountId = Sessions.FindChecked(Previous);
    Sessions.Remove(Previous);
    Sessions.Add(Next, AccountId);
    return true;
}

bool UTDAccountSubSystem::RegisterAccount(ATDPlayerState* Player, const FString& LoginId, const FString& Password, FString& OutError)
{
    if (!IsValid(Player) || !Player->HasAuthority() || FindAccount(Player) || Player->HasSelectedCharacter())
    { OutError = TEXT("로그아웃 상태에서 가입해 주세요."); return false; }
    if (LoginId.Len() < 2 || LoginId.Len() > 24 || Password.Len() < 4 || Password.Len() > 64)
    { OutError = TEXT("아이디는 2~24자, 비밀번호는 4~64자로 입력해 주세요."); return false; }
    for (TCHAR Ch : LoginId)
        if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == '_'))
        { OutError = TEXT("아이디는 영문, 숫자, 밑줄만 사용할 수 있습니다."); return false; }
    for (const auto& Account : Accounts)
        if (Account.LoginId.Equals(LoginId, ESearchCase::IgnoreCase))
        { OutError = TEXT("이미 사용 중인 아이디입니다."); return false; }
    FTDDummyAccountRecord Account;
    Account.AccountId = FGuid::NewGuid(); Account.LoginId = LoginId; Account.Password = Password;
    Accounts.Add(MoveTemp(Account)); OutError.Empty(); return true;
}

bool UTDAccountSubSystem::CreateCharacter(ATDPlayerState* Player, const FString& Name, FName ClassId, FString& OutError)
{
    const FTDDummyAccountRecord* Account = FindAccount(Player);
    if (!Account || Player->HasSelectedCharacter())
    { OutError = TEXT("로그인 후 캐릭터 목록에서 생성해 주세요."); return false; }
    if (Account->Characters.Num() >= MaxCharacters)
    { OutError = TEXT("캐릭터는 최대 6개까지 생성할 수 있습니다."); return false; }
    if (Name.Len() < 2 || Name.Len() > 16)
    { OutError = TEXT("캐릭터 이름은 2~16자로 입력해 주세요."); return false; }
    for (TCHAR Ch : Name)
        if (!(FChar::IsAlnum(Ch) || (Ch >= 0xAC00 && Ch <= 0xD7A3) || Ch == '_'))
        { OutError = TEXT("이름은 한글, 영문, 숫자, 밑줄만 사용할 수 있습니다."); return false; }
    UDataTable* Classes = GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous();
    if (!Classes || !Classes->FindRow<FTDCharacterClassRow>(ClassId, TEXT("CreateCharacter")))
    { OutError = TEXT("사용할 수 없는 직업입니다."); return false; }
    for (const auto& ExistingAccount : Accounts)
        for (const auto& Character : ExistingAccount.Characters)
            if (Character.CharacterName.Equals(Name, ESearchCase::IgnoreCase))
            { OutError = TEXT("이미 사용 중인 캐릭터 이름입니다."); return false; }
    FTDDummyCharacterRecord Character;
    Character.CharacterId = FGuid::NewGuid(); Character.CharacterName = Name;
    Character.Data.ClassId = ClassId;
    Character.Data.QuickSlots.SetNum(6);
    FTDDummyAccountRecord* Mutable = Accounts.FindByPredicate([&](const auto& Entry) { return Entry.AccountId == Account->AccountId; });
    Mutable->Characters.Add(MoveTemp(Character)); OutError.Empty(); return true;
}

bool UTDAccountSubSystem::DeleteCharacter(ATDPlayerState* Player, const FGuid& CharacterId, FString& OutError)
{
    const FTDDummyAccountRecord* Account = FindAccount(Player);
    if (!Account || Player->HasSelectedCharacter())
    { OutError = TEXT("캐릭터 선택 화면에서만 삭제할 수 있습니다."); return false; }
    if (!CharacterId.IsValid())
    { OutError = TEXT("삭제할 캐릭터를 다시 선택해 주세요."); return false; }
    FTDDummyAccountRecord* Mutable = Accounts.FindByPredicate([&](const auto& Entry) { return Entry.AccountId == Account->AccountId; });
    const int32 Index = Mutable->Characters.IndexOfByPredicate([&](const auto& Character) { return Character.CharacterId == CharacterId; });
    if (Index == INDEX_NONE)
    { OutError = TEXT("해당 계정에 캐릭터가 없습니다. 목록을 다시 확인해 주세요."); return false; }
    Mutable->Characters.RemoveAt(Index);
    OutError.Empty(); return true;
}
