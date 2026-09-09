#include "Save/Dummy/TDAccountDummyData.h"
#include "Misc/Optional.h"

#if !UE_BUILD_SHIPPING
namespace
{
    // 공통 아이템/퀵슬롯 구성. 계정별 초기값은 아래 CreateAccounts에서 직접 수정한다.
    FTDDummyCharacterRecord MakeCharacter(uint32 AccountNumber, uint32 CharacterNumber,
        const TCHAR* Name, const TCHAR* ClassId, int32 Level, int32 Exp, int32 Gold,
        float HealthRatio, int32 PotionCount, bool bEquipRing,
        const TCHAR* ZoneId, TOptional<FVector> Location)
    {
        FTDDummyCharacterRecord Character;
        Character.CharacterId = FGuid(0x54444443, 0, AccountNumber, CharacterNumber);
        Character.CharacterName = Name;
        FTDPlayerSaveData& Data = Character.Data;
        Data.ClassId = FName(ClassId);
        Data.Level = Level;
        Data.Exp = Exp;
        Data.Gold = Gold;
        Data.HealthRatio = HealthRatio;
        Data.ManaRatio = 1.f;
        Data.LastZoneId = FGameplayTag::RequestGameplayTag(FName(ZoneId));
        Data.bHasSavedLocation = Location.IsSet();
        Data.LastLocation = Location.Get(FVector::ZeroVector);

        FTDItemInstance& Potion = Data.InventoryItems.AddDefaulted_GetRef();
        Potion.ItemId = TEXT("HPotion_Low");
        Potion.SlotIndex = 0;
        Potion.Count = PotionCount;
        if (bEquipRing)
        {
            FTDItemInstance& Ring = Data.EquippedItems.AddDefaulted_GetRef();
            Ring.ItemId = TEXT("FireRing_Low");
            Ring.SlotIndex = 0;
        }
        Data.QuickSlots.SetNum(6);
        Data.QuickSlots[0].Type = ETDQuickSlotType::Item;
        Data.QuickSlots[0].Id = Potion.ItemId;
        return Character;
    }
}
#endif

TArray<FTDDummyAccountRecord> TDAccountDummyData::CreateAccounts()
{
    TArray<FTDDummyAccountRecord> Accounts;
#if !UE_BUILD_SHIPPING
    // 계정 1. 비밀번호는 공개된 개발용 값이다.
    FTDDummyAccountRecord Test01;
    Test01.AccountId = FGuid(0x54444441, 0, 0, 1);
    Test01.LoginId = TEXT("test01");
    Test01.Password = TEXT("dummy1234");
    // 계정 번호, 캐릭터 번호, 이름, 직업, 레벨, 경험치, 골드, 체력 비율, 포션 수, 반지 장착, 마지막 존, 마지막 좌표
    // 좌표 {}는 미저장(존 시작점). FVector(100.f, 200.f, 96.f)를 넣으면 해당 좌표로 복원한다.
    Test01.Characters = {
        MakeCharacter(1, 1, TEXT("테스트1_1"), TEXT("Warrior"), 1,    0, 10000, 1.00f, 10, false, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(1, 2, TEXT("테스트1_2"), TEXT("Mage"),    2,   80, 11000, 0.95f, 11, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(1, 3, TEXT("테스트1_3"), TEXT("Archer"),  3,  310, 12000, 0.90f, 12, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(1, 4, TEXT("테스트1_4"), TEXT("Warrior"), 4,  750, 13000, 0.85f, 13, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(1, 5, TEXT("테스트1_5"), TEXT("Mage"),    5, 1440, 14000, 0.80f, 14, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(1, 6, TEXT("테스트1_6"), TEXT("Archer"),  6, 2410, 15000, 0.75f, 15, true, TEXT("Zone.Region1.Town"), {}),
    };
    Accounts.Add(MoveTemp(Test01));

    // 계정 2. test01과 별개로 수정할 수 있다.
    FTDDummyAccountRecord Test02;
    Test02.AccountId = FGuid(0x54444441, 0, 0, 2);
    Test02.LoginId = TEXT("test02");
    Test02.Password = TEXT("dummy1234");
    Test02.Characters = {
        MakeCharacter(2, 1, TEXT("테스트2_1"), TEXT("Warrior"), 1,    0, 20000, 1.00f, 10, false, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(2, 2, TEXT("테스트2_2"), TEXT("Mage"),    2,   80, 21000, 0.95f, 11, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(2, 3, TEXT("테스트2_3"), TEXT("Archer"),  3,  310, 22000, 0.90f, 12, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(2, 4, TEXT("테스트2_4"), TEXT("Warrior"), 4,  750, 23000, 0.85f, 13, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(2, 5, TEXT("테스트2_5"), TEXT("Mage"),    5, 1440, 24000, 0.80f, 14, true, TEXT("Zone.Region1.Town"), {}),
        MakeCharacter(2, 6, TEXT("테스트2_6"), TEXT("Archer"),  6, 2410, 25000, 0.75f, 15, true, TEXT("Zone.Region1.Town"), {}),
    };
    Accounts.Add(MoveTemp(Test02));
#endif
    return Accounts;
}
