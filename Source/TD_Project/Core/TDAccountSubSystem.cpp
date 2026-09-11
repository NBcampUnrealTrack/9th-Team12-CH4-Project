#include "Core/TDAccountSubSystem.h"
#include "Player/TDPlayerState.h"
#include "Save/Dummy/TDAccountDummyData.h"

void UTDAccountSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
    Accounts = TDAccountDummyData::CreateAccounts();
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
