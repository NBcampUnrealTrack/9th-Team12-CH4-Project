#include "Save/Backend/TDBackendSaveCodec.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr double MaxExactInteger = 9007199254740991.0;
TSharedPtr<FJsonValue> WriteValue(FProperty *P, const void *V);
bool ReadValue(FProperty *P, void *V, const TSharedPtr<FJsonValue> &J);

// FName display casing can depend on name-pool insertion order in packaged builds.
// Compare property identities as FNames, but emit literal API contract spellings.
const TCHAR *WireFieldName(const FProperty *Property)
{
    struct FWireField
    {
        FName PropertyName;
        const TCHAR *JsonName;
    };
    static const FWireField Fields[] = {
#define TD_SAVE_FIELD(Name) {FName(TEXT(#Name)), TEXT(#Name)}
        TD_SAVE_FIELD(SaveVersion), TD_SAVE_FIELD(ClassId), TD_SAVE_FIELD(Level),
        TD_SAVE_FIELD(Exp), TD_SAVE_FIELD(SkillLevels), TD_SAVE_FIELD(InventorySlotCapacity),
        TD_SAVE_FIELD(InventoryItems), TD_SAVE_FIELD(EquippedItems), TD_SAVE_FIELD(HealthRatio),
        TD_SAVE_FIELD(ManaRatio), TD_SAVE_FIELD(LastZoneId), TD_SAVE_FIELD(LastLocation),
        TD_SAVE_FIELD(bHasSavedLocation), TD_SAVE_FIELD(QuestProgressTags), TD_SAVE_FIELD(ClaimedChestIds),
        TD_SAVE_FIELD(ChestClaimRecords), TD_SAVE_FIELD(SeenChapterIds), TD_SAVE_FIELD(NpcGiftRecords),
        TD_SAVE_FIELD(QuickSlots), TD_SAVE_FIELD(Gold), TD_SAVE_FIELD(QuestStates),
        TD_SAVE_FIELD(AffectionStates), TD_SAVE_FIELD(ItemId), TD_SAVE_FIELD(SlotIndex),
        TD_SAVE_FIELD(Count), TD_SAVE_FIELD(EnhanceLevel), TD_SAVE_FIELD(OptionRarity),
        TD_SAVE_FIELD(Options), TD_SAVE_FIELD(OptionId), TD_SAVE_FIELD(Value),
        TD_SAVE_FIELD(SkillId), TD_SAVE_FIELD(ChestId), TD_SAVE_FIELD(LastClaimKstDayKey),
        TD_SAVE_FIELD(NPCId), TD_SAVE_FIELD(LastGiftKstDayKey), TD_SAVE_FIELD(Points),
        TD_SAVE_FIELD(QuestId), TD_SAVE_FIELD(StateTag), TD_SAVE_FIELD(ObjectiveProgress),
        TD_SAVE_FIELD(AcceptSequence), TD_SAVE_FIELD(CompletedKstDayKey), TD_SAVE_FIELD(Type),
        TD_SAVE_FIELD(Id)
#undef TD_SAVE_FIELD
    };
    for (const FWireField &Field : Fields)
        if (Property->GetFName() == Field.PropertyName)
            return Field.JsonName;
    // A new save property must be added to the API contract explicitly.
    return nullptr;
}

TSharedPtr<FJsonObject> WriteStruct(const UStruct *S, const void *V)
{
    auto J = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(S); It; ++It)
    {
        // The save payload consists of editable data fields; FastArray bookkeeping is excluded.
        if (!It->HasAnyPropertyFlags(CPF_Edit))
            continue;
        const TCHAR *Name = WireFieldName(*It);
        if (!Name)
            return nullptr;
        auto Field = WriteValue(*It, It->ContainerPtrToValuePtr<void>(V));
        if (!Field)
            return nullptr;
        J->SetField(Name, Field);
    }
    return J;
}
bool ReadStruct(const UStruct *S, void *V, const TSharedPtr<FJsonObject> &J)
{
    if (!J)
        return false;
    int32 Count = 0;
    for (TFieldIterator<FProperty> It(S); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit))
            continue;
        ++Count;
        const TCHAR *Name = WireFieldName(*It);
        if (!Name)
            return false;
        auto Field = J->TryGetField(Name);
        if (!Field || !ReadValue(*It, It->ContainerPtrToValuePtr<void>(V), Field))
            return false;
    }
    return Count == J->Values.Num();
}
TSharedPtr<FJsonValue> WriteValue(FProperty *P, const void *V)
{
    if (auto *A = CastField<FArrayProperty>(P))
    {
        FScriptArrayHelper H(A, V);
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < H.Num(); ++I)
        {
            auto E = WriteValue(A->Inner, H.GetRawPtr(I));
            if (!E)
                return nullptr;
            Values.Add(E);
        }
        return MakeShared<FJsonValueArray>(Values);
    }
    if (auto *S = CastField<FStructProperty>(P))
    {
        if (S->Struct == FGameplayTag::StaticStruct())
        {
            const auto &Tag = *static_cast<const FGameplayTag *>(V);
            return MakeShared<FJsonValueString>(Tag.IsValid() ? Tag.ToString() : FString());
        }
        if (S->Struct == FGameplayTagContainer::StaticStruct())
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            TArray<FString> Names;
            for (const auto &Tag : *static_cast<const FGameplayTagContainer *>(V))
                Names.Add(Tag.ToString());
            Names.Sort();
            for (const auto &Name : Names)
                Values.Add(MakeShared<FJsonValueString>(Name));
            return MakeShared<FJsonValueArray>(Values);
        }
        if (S->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector &Pos = *static_cast<const FVector *>(V);
            if (!FMath::IsFinite(Pos.X) || !FMath::IsFinite(Pos.Y) || !FMath::IsFinite(Pos.Z))
                return nullptr;
            auto J = MakeShared<FJsonObject>();
            J->SetNumberField(TEXT("X"), Pos.X);
            J->SetNumberField(TEXT("Y"), Pos.Y);
            J->SetNumberField(TEXT("Z"), Pos.Z);
            return MakeShared<FJsonValueObject>(J);
        }
        auto J = WriteStruct(S->Struct, V);
        if (!J)
            return nullptr;
        return MakeShared<FJsonValueObject>(J);
    }
    if (auto *E = CastField<FEnumProperty>(P))
        return MakeShared<FJsonValueString>(
            E->GetEnum()->GetNameStringByValue(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(V)));
    if (auto *B = CastField<FBoolProperty>(P))
        return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(V));
    if (auto *N = CastField<FNameProperty>(P))
    {
        auto Name = N->GetPropertyValue(V);
        return MakeShared<FJsonValueString>(Name.IsNone() ? FString() : Name.ToString());
    }
    if (auto *N = CastField<FNumericProperty>(P))
    {
        const double Number =
            N->IsInteger() ? static_cast<double>(N->GetSignedIntPropertyValue(V)) : N->GetFloatingPointPropertyValue(V);
        if (!FMath::IsFinite(Number) || (N->IsInteger() && FMath::Abs(Number) > MaxExactInteger))
            return nullptr;
        return MakeShared<FJsonValueNumber>(Number);
    }
    return nullptr;
}
bool ReadValue(FProperty *P, void *V, const TSharedPtr<FJsonValue> &J)
{
    if (!J)
        return false;
    if (auto *A = CastField<FArrayProperty>(P))
    {
        if (J->Type != EJson::Array || J->AsArray().Num() > 10000)
            return false;
        FScriptArrayHelper H(A, V);
        H.Resize(J->AsArray().Num());
        for (int32 I = 0; I < H.Num(); ++I)
            if (!ReadValue(A->Inner, H.GetRawPtr(I), J->AsArray()[I]))
                return false;
        return true;
    }
    if (auto *S = CastField<FStructProperty>(P))
    {
        if (S->Struct == FGameplayTag::StaticStruct())
        {
            FString Name;
            if (!J->TryGetString(Name))
                return false;
            // Legacy saves serialized the empty tag's FName as "None".
            if (Name == TEXT("None"))
                Name.Empty();
            auto Tag = Name.IsEmpty() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(FName(*Name), false);
            if (!Name.IsEmpty() && !Tag.IsValid())
                return false;
            *static_cast<FGameplayTag *>(V) = Tag;
            return true;
        }
        if (S->Struct == FGameplayTagContainer::StaticStruct())
        {
            if (J->Type != EJson::Array)
                return false;
            auto &Tags = *static_cast<FGameplayTagContainer *>(V);
            Tags.Reset();
            for (const auto &Value : J->AsArray())
            {
                FString Name;
                if (!Value->TryGetString(Name) || Name.IsEmpty())
                    return false;
                auto Tag = FGameplayTag::RequestGameplayTag(FName(*Name), false);
                if (!Tag.IsValid() || Tags.HasTagExact(Tag))
                    return false;
                Tags.AddTag(Tag);
            }
            return true;
        }
        if (J->Type != EJson::Object)
            return false;
        if (S->Struct == TBaseStructure<FVector>::Get())
        {
            double X, Y, Z;
            auto O = J->AsObject();
            if (O->Values.Num() != 3 || !O->TryGetNumberField(TEXT("X"), X) || !O->TryGetNumberField(TEXT("Y"), Y) ||
                !O->TryGetNumberField(TEXT("Z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) ||
                !FMath::IsFinite(Z))
                return false;
            *static_cast<FVector *>(V) = FVector(X, Y, Z);
            return true;
        }
        return ReadStruct(S->Struct, V, J->AsObject());
    }
    if (auto *E = CastField<FEnumProperty>(P))
    {
        FString Name;
        if (!J->TryGetString(Name))
            return false;
        auto Index = E->GetEnum()->GetIndexByNameString(Name);
        if (Index == INDEX_NONE || E->GetEnum()->GetNameStringByIndex(Index) != Name || Name.EndsWith(TEXT("_MAX")))
            return false;
        E->GetUnderlyingProperty()->SetIntPropertyValue(V, E->GetEnum()->GetValueByIndex(Index));
        return true;
    }
    if (auto *B = CastField<FBoolProperty>(P))
    {
        bool Value;
        if (!J->TryGetBool(Value))
            return false;
        B->SetPropertyValue(V, Value);
        return true;
    }
    if (auto *N = CastField<FNameProperty>(P))
    {
        FString Name;
        if (!J->TryGetString(Name) || Name.Len() > 256)
            return false;
        N->SetPropertyValue(V, FName(*Name));
        return true;
    }
    if (auto *N = CastField<FNumericProperty>(P))
    {
        double Number;
        if (J->Type != EJson::Number || !J->TryGetNumber(Number) || !FMath::IsFinite(Number))
            return false;
        if (N->IsInteger())
        {
            if (FMath::Abs(Number) > MaxExactInteger || Number != FMath::FloorToDouble(Number))
                return false;
            if (P->GetSize() == 4 && (Number < MIN_int32 || Number > MAX_int32))
                return false;
            N->SetIntPropertyValue(V, static_cast<int64>(Number));
        }
        else
        {
            if (P->GetSize() == 4 && FMath::Abs(Number) > MAX_flt)
                return false;
            N->SetFloatingPointPropertyValue(V, Number);
        }
        return true;
    }
    return false;
}
} // namespace
TSharedPtr<FJsonObject> TDBackendSaveCodec::Encode(const FTDPlayerSaveData &Data)
{
    return WriteStruct(FTDPlayerSaveData::StaticStruct(), &Data);
}
bool TDBackendSaveCodec::Decode(const TSharedPtr<FJsonObject> &Json, FTDPlayerSaveData &Out)
{
    FTDPlayerSaveData Temp;
    if (!ReadStruct(FTDPlayerSaveData::StaticStruct(), &Temp, Json) || Temp.SaveVersion != 2 || Temp.HealthRatio < 0 ||
        Temp.HealthRatio > 1 || Temp.ManaRatio < 0 || Temp.ManaRatio > 1)
        return false;
    Out = MoveTemp(Temp);
    return true;
}
