# TD_Project — 2.5D 멀티플레이 MMORPG

내일배움캠프 언리얼 9기 Team 12 최종 프로젝트 · Unreal Engine 5.8 · 2026-08-21 ~ 2026-09-17 · 5인

직업 3종(전사·궁수·마법사)으로 필드를 사냥하며 성장하고, 파티를 맺어 보스방에 입장하는 쿼터뷰 MMORPG입니다.
데디케이티드 서버가 모든 판정을 하고, 계정·캐릭터·세이브는 별도 백엔드(Rust + PostgreSQL)에 저장합니다.

| 목차 | |
|---|---|
| [1. Project Architecture](#1-project-architecture) | [5. Replication Strategy](#5-replication-strategy) |
| [2. Network Architecture](#2-network-architecture) | [6. DataTable Structure](#6-datatable-structure) |
| [3. Class Responsibility](#3-class-responsibility) | [7. Dedicated Server 실행 방법](#7-dedicated-server-실행-방법) |
| [4. Gameplay Framework](#4-gameplay-framework) | [8. Build 방법](#8-build-방법) |

팀 구성과 결과 요약은 [Docs/PROJECT_OVERVIEW.md](Docs/PROJECT_OVERVIEW.md)에 있습니다.

---

## 1. Project Architecture

```
┌────────────┐  UE 넷드라이버(UDP 7777)  ┌──────────────────┐  HTTP(JSON)  ┌──────────┐      ┌────────────┐
│ Client     │ ───────────────────────▶ │ Dedicated Server │ ───────────▶ │ Rust API │ ───▶ │ PostgreSQL │
│ (Windows)  │ ◀─────────────────────── │ (판정·AI·스폰)    │ ◀─────────── │ (Axum)   │      │            │
└────────────┘   복제 · Multicast         └──────────────────┘              └──────────┘      └────────────┘
```

| 구성 요소 | 위치 | 역할 |
|---|---|---|
| 게임 모듈 `TD_Project` | `Source/TD_Project` | 게임 로직 전부. 클라이언트와 서버가 같은 코드를 쓰고 권한으로 갈린다 |
| 프리로드 모듈 `TD_ProjectPreLoad` | `Source/TD_ProjectPreLoad` | 스플래시 직후 로딩 화면 |
| 에디터 모듈 `TD_ProjectEditor` | `Source/TD_ProjectEditor` | Google Sheets → DataTable 파서. 패키지에 포함되지 않음 |
| 백엔드 | `API/` | Rust(Axum) + SQLx. 계정, 캐릭터, 세이브(리비전 검증) |
| 데이터 도구 | `Tools/` | 테이블 생성·내보내기·검증 파이썬 스크립트 |
| 콘텐츠 | `Content/` | 블루프린트, 데이터 테이블, 레벨(World Partition), UI, 아트 |

`Source/TD_Project` 폴더 구성 (C++ 약 63,500줄):

| 폴더 | 내용 |
|---|---|
| `Game`, `Core` | GameMode, GameState, GameInstance, 서브시스템, 게임플레이 태그, 디버그 치트 |
| `Player`, `Character` | PlayerController, PlayerState, 캐릭터 베이스, 플레이어·몬스터·보스 |
| `Combat`, `AI`, `Abilities` | 데미지 파이프라인, 전투 컴포넌트, 몬스터 FSM, 보스 BT·패턴, AttributeSet |
| `Stats`, `Skill` | 스탯 계산, 레벨·경험치, 스킬 시전 |
| `Items`, `Enhance`, `Option`, `Shop`, `Market` | 인벤토리, 장착, 강화, 옵션, 상점, 거래소 |
| `Quest`, `Interaction`, `World` | 퀘스트, NPC 대화, 포탈, 보물상자, 존 |
| `Party`, `Chat` | 파티, 채팅 |
| `Save` | 세이브 데이터, 백엔드 HTTP 서브시스템, JSON 코덱 |
| `UI` | 위젯, UI 매니저, 뷰모델 |
| `Data`, `Settings` | 데이터 테이블 행 구조체, 프로젝트 설정 클래스 |

사용 플러그인: GameplayAbilities, CommonUI, ModelViewViewModel, PaperZD, AudioModulation, GoogleSheetLoader(프로젝트 포함).

---

## 2. Network Architecture

**서버 권한(Server-Authoritative)** 구조입니다. 클라이언트는 입력과 요청만 보내고, 검증과 결과 확정은 서버가 합니다.

![요청 → 검증 → 결과 복제](Docs/images/01_server_authority_sequence.png)

| 원칙 | 구현 |
|---|---|
| 판정은 서버만 | 피해·사망·보상·스폰·아이템·강화·존 이동이 모두 `HasAuthority()` 경로 |
| 요청에 결과를 싣지 않는다 | `ServerRequestAttack()`처럼 인자에 피해량·대상이 없다. 쿨타임·사거리는 서버 값으로 검증 |
| 실패는 조용히 무시하거나 사유 코드로 응답 | 존 이동 거부는 enum 사유를 Client RPC로 보내고 문구는 UI가 정한다 |
| 백엔드는 서버만 호출 | 저장 API는 `X-Game-Server-Key`가 필요하고 키는 서버 환경 변수에만 둔다 |

**월드와 방 배정**

- World Partition 단일 맵(`GameTestLevel`)에 마을·필드·보스방이 모두 있고, 서버가 **존(Zone) 태그** 단위로 플레이어를 이동시킵니다.
- 필드는 공용입니다. 보스방은 같은 맵 안의 방 4개를 서버가 **파티 단위**로 배정합니다(빈 방 우선, 파티원 합류, 재입장 차단, 가득 차면 거부).
- 방이 새 파티에 배정되면 그 방의 스폰포인트를 초기화해 보스가 처음 상태로 돌아갑니다.

**접속 흐름**

1. 클라이언트 실행 → `UTDGameInstance`가 `DefaultServerAddress`(`Config/DefaultGame.ini`)로 자동 접속
2. 서버 접속 후 로그인·캐릭터 선택 UI 표시 → 서버가 백엔드에 인증·캐릭터 조회
3. 캐릭터 선택 시 서버가 세이브를 읽어 PlayerState에 적용하고 Pawn 스폰

---

## 3. Class Responsibility

| 클래스 | 위치 | 책임 |
|---|---|---|
| `ATDGameMode` | `Game/` | 서버 전용 규칙. 존 이동 요청 검증, 보스방 배정, 리스폰 지점 결정 |
| `ATDGameState` | `Game/` | 전원이 알아야 하는 월드 상태(현재 보스 `ActiveBoss` 등) 복제 |
| `UTDGameInstance` | `Core/` | 서버 접속(`ConnectToServer`), 사용자 옵션 적용 |
| `ATDPlayerController` | `Player/` | 입력, 서버 RPC 창구(계정·채팅·상호작용), Client RPC 수신 |
| `ATDPlayerState` | `Player/` | 리스폰해도 남아야 하는 데이터의 소유자. ASC와 기능 컴포넌트 8개 보유 |
| `ATDCharacterBase` | `Character/` | 플레이어·몬스터 공통 베이스. 사망, 피격 알림, 팀, 체력 재생 |
| `ATDPlayerCharacter` | `Character/` | 월드의 "몸". 데이터는 갖지 않고 PlayerState에 물어본다 |
| `ATDEnemyBase` | `Character/` | 몬스터 베이스. 스탯·ASC를 자기가 소유, 보상·드랍 지급 |
| `ATDBossCharacter` | `Character/` | 보스 패턴 엔진(예고→타격→후딜), 페이즈, 광폭화, 귀환 |
| `UTDCombatComponent` | `Combat/` | 공격 요청 → 검증 → 모션 방송 → 판정 |
| `UTDCombatStatics` / `TDCombatCalculation` | `Combat/` | 피해 계산(순수 함수)과 적용. 모든 피해의 단일 입구 |
| `UTDAttributeSet` | `Abilities/` | 체력·마나 어트리뷰트, 피해 적용 훅 |
| `UTDStatComponent` | `Stats/` | 태그 기반 스탯. 장비·버프가 넘긴 모디파이어를 소스 단위로 합산 |
| `UTDProgressionComponent` | `Stats/` | 레벨, 경험치, 직업, 스킬 레벨 |
| `UTDSkillComponent` | `Skill/` | 액티브 스킬 시전과 쿨타임. Pawn에 부착 |
| `UTDInventoryComponent` / `UTDItemUseComponent` | `Items/` | 가방·골드 / 장착·사용·강화 |
| `UTDPartyComponent` | `Party/` | 파티 ID와 리더 여부, 경험치·골드 분배 |
| `UTDQuestComponent` | `Quest/` | 퀘스트 진행과 보상 |
| `ATDMonsterAIController` | `AI/` | 필드 몬스터 FSM(대기·배회·발견·전투) |
| `ATDBossAIController` + BT 노드 | `AI/` | 보스 Behavior Tree. 대상 선택, 패턴 선택·실행 |
| `ATDSpawnPoint` / `UTDSpawnSubsystem` | `AI/` | 몬스터·보스 스폰과 리젠 / 존 단위 초기화 |
| `ATDPortal` | `World/` | 겹침만 감지하고 `GameMode`에 이동을 요청 |
| `UTDBackendSaveSubsystem` | `Save/Backend/` | 서버 전용 HTTP 계정·세이브 통신 |
| `UTDUIManagerSubsystem` | `UI/Core/` | 로컬 플레이어의 화면 흐름과 창 관리 |

---

## 4. Gameplay Framework

언리얼의 기본 틀에 역할을 이렇게 배치했습니다. 기준은 **수명**과 **누가 알아야 하는가**입니다.

| 프레임워크 클래스 | 존재 위치 | 우리가 둔 것 | 이유 |
|---|---|---|---|
| GameMode | 서버만 | 존 이동, 방 배정, 리스폰 | 클라이언트가 알 필요도, 알아서도 안 되는 규칙 |
| GameState | 서버 + 전 클라 | `ActiveBoss`, 존 정보 | UI가 읽어야 하는 월드 상태 |
| PlayerController | 서버 + 소유 클라 | 입력, RPC 창구 | 본인만 쓰는 통로 |
| PlayerState | 서버 + 전 클라 | ASC, 스탯, 성장, 인벤토리, 장착, 파티, 퀘스트, 퀵슬롯 | 죽어서 Pawn이 파괴돼도 남아야 함 |
| Pawn(Character) | 서버 + 전 클라 | 전투·스킬 실행, 스프라이트, 이름표 | 죽으면 사라지는 "몸" |
| HUD / UI | 클라만 | 위젯, UI 매니저 서브시스템 | 서버에는 화면이 없음 |

![PlayerState 중심 구조](Docs/images/03_playerstate_lifecycle.png)

**Gameplay Ability System 사용 범위**

- 사용: `AbilitySystemComponent`, `AttributeSet`(Health, MaxHealth, Mana, MaxMana), 피해 적용용 일회용 `GameplayEffect`(C++에서 생성)
- 사용하지 않음: GameplayAbility, GE·GA 에셋. 스탯 계산·버프·스킬·쿨타임은 데이터 테이블 기반 자체 컴포넌트
- 플레이어: Owner = PlayerState, Avatar = 현재 캐릭터. 새 캐릭터가 붙을 때 서버는 `PossessedBy`, 클라는 `OnRep_PlayerState`에서 `InitAbilityActorInfo`
- 몬스터·보스: Owner = Avatar = 자기 자신

![ASC Owner와 Avatar](Docs/images/08_gas_owner_avatar.png)

---

## 5. Replication Strategy

"무엇을, 누구에게, 어떤 통로로" 보낼지를 종류별로 정했습니다.

| 종류 | 통로 | 예 |
|---|---|---|
| 클라 → 서버 요청 | Server RPC (68개) | 공격, 아이템 사용, 강화, 파티 초대, 존 이동 |
| 서버 → 특정 클라 통지 | Client RPC (34개) | 요청 결과, 거부 사유, 채팅 수신 |
| 한 번 터지는 연출 | Multicast RPC (18개) | 공격 모션, 타격 VFX, 보스 예고, 카메라 흔들림 |
| 계속 유지되는 상태 | 복제 프로퍼티 + RepNotify (29개) | 체력, 사망, 직업, 현재 존, 보스 페이즈 |

**규칙**

1. **상태는 복제, 사건은 RPC.** 늦게 들어온 플레이어도 알아야 하면 복제 프로퍼티로 둡니다. 놓쳐도 어긋나지 않는 연출만 Multicast를 씁니다.
2. **본인만 필요한 값은 `COND_OwnerOnly`.** 스탯 상세, 인벤토리, 골드, 장착, 퀵슬롯, 퀘스트, 캐릭터 목록. 전투력·직업·존처럼 남에게 보여야 하는 값만 전원에게 보냅니다.
3. **자주 바뀌는 배열은 Fast Array Serializer.** 인벤토리와 장착 목록은 바뀐 항목만 전송하므로 가방 크기와 전송량이 비례하지 않습니다.
4. **ASC 복제 모드 구분.** 플레이어는 `Mixed`(어트리뷰트는 전원, 이펙트 상세는 본인), 몬스터는 `Minimal`.
5. **그리기는 클라이언트가.** 보스 바닥 예고는 패턴 번호·위치·시간만 Multicast로 보내고 데칼은 각 클라이언트가 스폰합니다. 예고 액터 자체는 복제하지 않습니다.
6. **PlayerState 갱신 빈도 상향.** 기본 1Hz에서 100으로 올려 체력바가 끊겨 보이지 않게 했습니다.
7. **레벨에 배치된 액터는 복제하지 않는다.** 포탈은 양쪽 월드에 이미 있으므로 서버가 겹침만 판정합니다.

![조건부 복제](Docs/images/05_conditional_replication_code.png)

---

## 6. DataTable Structure

게임 수치는 `Content/Data/DataTables`의 테이블 38개에 있습니다. 행 구조체는 `Source/TD_Project/Data`에 정의돼 있고, 표끼리는 **행 이름(ID)** 으로 참조합니다.

| 묶음 | 테이블 | 행 구조체 예 |
|---|---|---|
| 캐릭터·성장 | `DT_CharacterClass`, `DT_ClassGrowth`, `DT_LevelExp`, `DT_StatDefinition`, `DT_UnionBonus` | `FTDClassGrowthRow`, `FTDStatRow` |
| 몬스터 | `DT_MonsterDefinition` | `FTDMonsterRow` |
| 스킬 | `DT_Skill`, `DT_SkillEffect`, `DT_SkillPassive` | 스킬 정의 / 효과 / 패시브 스탯 |
| 아이템 | `DT_ItemDefinition`, `DT_ItemStat`, `DT_ItemSet`, `DT_ItemSetBonus`, `DT_ItemUseEffect` | 아이템 정의와 스탯은 분리 |
| 옵션·강화·드랍 | `DT_OptionDefinition`, `DT_OptionPool`, `DT_OptionRarity`, `DT_Enhance`, `DT_DropTable` | |
| 월드·퀘스트·상점 | `DT_ZoneEnvironment`, `DT_NPC`, `DT_Dialogue`, `DT_Quest`, `DT_Chapter`, `DT_Shop`, `DT_ShopItem`, `DT_TreasureChest` | |

**참조 관계 예**

```
DT_MonsterDefinition.DropTableId ──▶ DT_DropTable.ItemId ──▶ DT_ItemDefinition
                                                              ├─▶ DT_ItemStat (ItemId)
                                                              └─▶ DT_ItemSet ──▶ DT_ItemSetBonus
DT_Skill (SkillId) ──▶ DT_SkillEffect / DT_SkillPassive
DT_ClassGrowth.ClassId ──▶ DT_CharacterClass
DT_ZoneEnvironment.ZoneId (게임플레이 태그) ──▶ PlayerStart 태그, 보스방 묶음
```

**설계 규칙**

- 중첩 배열 대신 표를 분리하고 ID로 잇습니다(예: 아이템과 아이템 스탯).
- 스탯·존·피해 타입은 문자열이 아니라 **게임플레이 태그**로 적습니다(`Stat.Offense.Damage.Physical`, `Zone.Region2.Boss.Room01`).
- 레벨에 따라 변하는 값은 `FScalableFloat`라 같은 행으로 여러 레벨을 표현합니다.
- 보스 패턴은 테이블이 아니라 보스 BP의 `FTDBossPatternSpec` 배열입니다(`Source/TD_Project/Combat/TDBossPattern.h`).

**파이프라인**

![시트에서 게임까지](Docs/images/10_sheet_pipeline.png)

| 작업 | 방법 |
|---|---|
| 시트 → 테이블 | 에디터 툴바 **Sheet Loader** → 해당 줄 **Update**. 로더 에셋은 `Content/Data/DataTables/GoogleSheet/GDA_*` |
| 규칙 기반 표 생성 | `Tools/GenItemTables.py`, `GenSkillTables.py`, `GenOptionTables.py` 실행 후 에디터에서 Reimport |
| CSV 내보내기 | `Tools/ExportTables.bat` |
| 검증 | `Tools/ValidateData.bat` (에디터를 닫고 실행. 교차 참조와 값 범위 검사) |

---

## 7. Dedicated Server 실행 방법

### 7-1. 백엔드 (선택)

백엔드 없이도 서버는 뜹니다. 그 경우 계정은 프로젝트에 포함된 더미 데이터로 동작합니다.

```powershell
cd API
./scripts/setup.ps1
docker compose --env-file .env.api up -d --build
curl.exe http://127.0.0.1:18080/health
```

자세한 API 명세는 [API/README.md](API/README.md)에 있습니다.

### 7-2. 서버 실행

패키징한 서버 폴더에서 실행합니다. 기본 맵은 `GameTestLevel`, 포트는 7777입니다.

```powershell
$env:TD_BACKEND_URL = "http://127.0.0.1:18080"
$env:TD_GAME_SERVER_KEY = "<API/.env.api 의 GAME_SERVER_API_KEY>"
.\TD_ProjectServer.exe -log -port=7777
```

- 두 환경 변수를 주지 않으면 백엔드 연동 없이 실행됩니다.
- 서버 키는 서버 프로세스에만 전달하고 클라이언트 빌드나 저장소에는 넣지 않습니다.

### 7-3. 클라이언트 접속

| 방법 | 설명 |
|---|---|
| 자동 접속 | 패키징된 클라이언트는 시작 시 `Config/DefaultGame.ini`의 `DefaultServerAddress`로 접속 |
| 주소 지정 실행 | `TD_Project.exe 127.0.0.1:7777 -log` |
| 실행 중 접속 | 콘솔(`` ` ``)에서 `TDConnect 127.0.0.1:7777` |

### 7-4. 에디터에서 서버-클라 테스트

Play 드롭다운 → Net Mode **Play As Client**, Number of Players 2 이상. 에디터가 데디케이티드 서버를 함께 띄웁니다.
치트는 콘솔에서 `TD.` 까지 입력하면 목록이 보입니다(예: `TD.GiveTestCharacters`, `TD.SelectCharacter 0`, `TD.SetLevel 50`, `TD.Zone Zone.Region2.Boss`).

---

## 8. Build 방법

### 요구 사항

| 항목 | 버전·비고 |
|---|---|
| Unreal Engine | 5.8. **Server 타깃을 빌드하려면 소스 빌드 엔진**이 필요합니다(런처 설치본은 Editor·Game 타깃만) |
| IDE | Visual Studio 2022 (C++ 게임 개발 워크로드) 또는 Rider |
| Git LFS | 필수. `.uasset`, `.umap`, `.png`, `.wav` 등 18종이 LFS로 관리됩니다 |
| Docker | 백엔드를 띄울 때만 |

### 클론과 에디터 빌드

```bash
git lfs install
git clone https://github.com/NBcampUnrealTrack/9th-Team12-CH4-Project.git
```

1. `TD_Project.uproject` 우클릭 → **Generate Visual Studio project files**
2. 솔루션을 열고 구성 `Development Editor | Win64`로 `TD_Project` 빌드
3. `TD_Project.uproject` 실행

명령줄 빌드:

```powershell
& "<UE_5.8>\Engine\Build\BatchFiles\Build.bat" TD_ProjectEditor Win64 Development -Project="<경로>\TD_Project.uproject" -WaitMutex
```

### 빌드 타깃

| 타깃 | 파일 | 용도 |
|---|---|---|
| `TD_ProjectEditor` | `Source/TD_ProjectEditor.Target.cs` | 에디터 |
| `TD_Project` | `Source/TD_Project.Target.cs` | 클라이언트 |
| `TD_ProjectServer` | `Source/TD_ProjectServer.Target.cs` | 데디케이티드 서버 |

### 패키징

| 대상 | 에디터 메뉴 |
|---|---|
| 클라이언트 | Platforms → Windows → Build Target `TD_Project` → Package Project |
| 서버 | Platforms → Windows → Build Target `TD_ProjectServer` → Package Project |

패키징 전 확인:

- `Tools/ValidateData.bat`으로 데이터 검증
- 내비메시는 레벨 담당자가 **Build → Build Paths**로 구워 커밋한 상태여야 몬스터가 이동합니다
- `Config/DefaultGame.ini`의 `DefaultServerAddress`가 접속할 서버 주소인지 확인

### 브랜치

기능 브랜치 → `dev`(PR) → `main`. 커밋에는 `*.slnx`와 `Config/DefaultEditor.ini` 같은 로컬 환경 파일을 포함하지 않습니다.
