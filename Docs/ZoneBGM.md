# Zone BGM 설정과 검증

## 기존 구조와 구현

- `TDPortal::TryTravel` → `TDGameMode::RequestZoneTravelInternal`이 입장 조건과 도착점을 검사하고 TeleportTo 성공 후 `TDPlayerState::SetCurrentZoneId`를 호출한다.
- 서버의 `OnZoneChanged`는 리슨 서버의 로컬 플레이어에 적용된다. 원격 클라이언트는 `OnRep_CurrentZoneId` → `OnZoneChanged`로 통지받는다. 새 RPC나 multicast는 필요하지 않다.
- 기존 `TDZoneEnvironmentComponent`가 자기 컨트롤러의 PlayerState만 구독한다. 이제 기존 카메라/라이팅 처리와 함께 BGM도 적용한다. 서버의 원격 컨트롤러와 전용 서버에서는 적용하지 않는다.
- BGM 데이터는 이미 존재하는 `DT_ZoneEnvironment.EnvironmentData` → `TDZoneEnvironmentData.bOverride_BGM/BGM`을 사용한다. 미지정 시 `TDZoneSettings.DefaultBGM`, override를 켜고 BGM을 비우면 무음이다.
- 같은 Sound 에셋을 재생 중이면 유지한다. 다른 에셋이면 Stop → SetSound → Play 순서이며 크로스페이드를 하지 않는다. AudioComponent는 컨트롤러마다 하나만 생성하고 재사용한다.
- 선택 완료, 유효한 Zone, Pawn, 계정 화면 닫힘을 확인한다. 복제 순서가 달라도 기존 Tick에서 준비 상태 변화를 감지해 적용한다. 로그아웃/캐릭터 선택 복귀 시 정지하고 EndPlay에서 컴포넌트와 구독을 정리한다.
- `TDGameInstance`는 기존대로 맵 로드 후 사용자 설정을 적용한다. `TDGameUserSettings`의 Control Bus 볼륨 경로를 그대로 사용한다. 포탈, GameMode, PlayerState, 볼륨 계산식, uasset은 수정하지 않았다.

## Unreal Editor 필수 설정

코드만으로 에셋 연결이나 반복 재생 설정은 바뀌지 않는다. 아래 설정을 완료해야 음악과 볼륨 옵션이 동작한다.

### 1. 네 Sound Wave

Content Browser의 `/Game/Audio/Class`에서 `Village1`, `Field1`, `Desert`, `Boss`를 각각 연다. Details 검색을 사용한다.

- `Looping`: 켬. 곡이 끝나도 반복한다.
- `Sound Submix`(Base Submix): 기존 `SM_BGM` 지정. 추가 Submix Send로 동시에 Master에 보내지 않는다.
- `Virtualization Mode`: `Play When Silent`. 볼륨 0에서도 재생 위치가 계속 진행되게 한다.
- 기존 Sound Class/Concurrency가 곡을 강제로 중지하거나 별도 출력으로 우회하지 않는지 확인하고 저장한다.

코드가 공간화를 끄므로 위치/거리 감쇠 없이 재생된다. 별도 볼륨 배율을 곱하지 않는다.

### 2. 기존 볼륨 경로

같은 폴더의 `SM_BGM`을 열어 `Output Volume Modulation`의 Volume Modulator에 `CB_BGM`이 들어 있는지 확인한다. `SM_Master`의 Child Submixes에 `SM_BGM`이 연결되어 있고, `SM_Master`의 Output Volume Modulation은 `CB_Master`를 사용하는지 확인한다. Control Bus의 Parameter는 Volume 계열이어야 한다.

Project Settings → TD → TD Audio의 네 버스는 DefaultGame.ini에 이미 지정되어 있다. BGM=`CB_BGM`, Master=`CB_Master`, SFX=`CB_SFX`, UI=`CB_UI`를 확인한다. 동일 버스를 Sound Wave와 Submix 양쪽에 중복 적용하지 않는다.

바이너리 에셋의 실제 내부 배선은 이번 코드 작업에서 검증하지 않았으므로 Editor에서 반드시 확인한다.

### 3. Zone 환경 DataAsset

Content Browser → `/Game/Data/DataAssets` → 우클릭 → Miscellaneous → Data Asset → `TDZoneEnvironmentData`를 선택한다. 다음 4개를 만든다.

| 새 에셋 이름 | Sound → BGM (옆 override 체크 켬) |
|---|---|
| DA_ZoneBGM_Village | Village1 |
| DA_ZoneBGM_Field | Field1 |
| DA_ZoneBGM_Desert | Desert |
| DA_ZoneBGM_Boss | Boss |

Camera와 Lighting override는 끈 상태로 둔다. **행에 기존 EnvironmentData가 있으면 새 에셋으로 덮어쓰지 말고 기존 에셋의 BGM만 설정**한다. 서로 다른 조명/카메라를 가진 에셋이라도 BGM 칸에서 같은 Sound Wave를 참조하면 음악은 유지된다.

`/Game/Data/DataTables/World/DT_ZoneEnvironment`를 열고 아래 행의 `EnvironmentData`를 연결한다. RowName과 실제 ZoneId는 별개이며, 코드는 ZoneId 열로 조회한다.

| 지역 | DataTable 행 | EnvironmentData | BGM |
|---|---|---|---|
| 1-0_Medieval_Village | R1_Town | DA_ZoneBGM_Village | Village1 |
| 1-1_Medieval_Field | R1_Field01 | DA_ZoneBGM_Field | Field1 |
| 1-2_Medieval_Field | R1_Field02 | DA_ZoneBGM_Field | Field1 |
| 1-3_Medieval_Field | R1_Field03 | DA_ZoneBGM_Field | Field1 |
| 1-4_Dungeon_Field | R1_Field04 | DA_ZoneBGM_Field | Field1 |
| 1-5_Dungeon_Boss1~4 | R1_Boss01~04 (각 행) | DA_ZoneBGM_Boss | Boss |
| 2-0_Desert_Village | R2_Town | DA_ZoneBGM_Desert | Desert |
| 2-1_Desert_Field | R2_Field01 | DA_ZoneBGM_Desert | Desert |
| 2-2_Desert_Field | R2_Field02 | DA_ZoneBGM_Desert | Desert |
| 2-3_Desert_Boss1~4 | R2_Boss01~04 (각 행) | DA_ZoneBGM_Boss | Boss |

Project Settings → TD → Zone 설정의 `Default BGM`은 비워 둔다. 테이블 참조는 기존 DT_ZoneEnvironment를 유지한다. 저장 후 다시 열어 연결을 확인한다.

현재 저장소의 `Content/Data/DataTables/World/CSV/GoogleSheet/DT_ZoneEnvironment.csv`는 모든 EnvironmentData가 None이다. CSV/GoogleSheet 재임포트를 사용한다면 원본의 해당 열에도 만든 에셋의 Copy Reference 값을 반영해야 연결이 지워지지 않는다. 에셋을 먼저 만든 후 실제 경로를 사용한다.

레벨에 새 AmbientSound를 배치하지 않는다. 기존 레벨/BP에 이 네 곡의 자동 재생 노드나 AmbientSound가 있다면 제거하거나 Auto Activate를 끈다. 단일 재생 보장은 이 컴포넌트가 관리하는 BGM에 적용되며 외부 BP에서 독립적으로 실행한 오디오는 제어하지 않는다.

## 테스트

코드 검증: UE 5.8 `TD_ProjectEditor Win64 Development` 빌드 성공, `git diff --check` 통과. 아래 Editor/청취/네트워크 시나리오는 에셋 설정 후 실행해야 하며 이번 작업에서 실행하지 않았다.

1. GameTestLevel 실행 후 로그인/캐릭터 선택 화면에서 네 BGM 모두 무음인지 확인한다. 신규 캐릭터 진입은 Village1, 저장 캐릭터 진입은 저장 Zone 음악이어야 한다.
2. 1-0 → 1-1에서 Village1이 멈추고 Field1만 재생되는지 확인한다. 1-1 → 1-2 → 1-3 → 1-4에서 곡의 진행 위치가 유지되는지 듣는다.
3. 2-0 → 2-1 → 2-2에서도 Desert가 유지되는지 확인한다. 양 지역의 보스방 8개는 Boss, 보스방 이탈은 목적지 음악이어야 한다.
4. 자동 포탈과 F키 포탈을 각각 시험한다. 레벨 부족, 막힌 도착점, 쿨다운 중에는 Zone과 음악이 바뀌지 않아야 한다. TargetEntryPoint/EntryName에 따른 도착 좌표도 확인한다.
5. 다른 곡 사이를 빠르게 왕복해 중첩이 없는지 확인한다. 같은 Zone 재적용도 재시작하지 않아야 한다. 곡 길이 이상 기다려 반복을 확인한다.
6. 사망/리스폰 시 목적지 Zone 음악인지 확인한다. 캐릭터 선택 복귀와 로그아웃에서 정지하고, 다른 캐릭터를 선택하면 그 캐릭터 Zone 음악인지 확인한다. 연결 종료/맵 이탈 때 잔류 음악이 없어야 한다.
7. BGM 볼륨 0/0.5/1 및 Master 0/1을 시험한다. SFX/UI 슬라이더가 BGM을 변경하지 않아야 한다. BGM을 0으로 했다 복원해 재생 위치가 진행되었는지 확인한다.
8. 전용 서버 + 클라이언트 2개를 **별도 프로세스**로 실행한다. A=1-1(Field1), B=2-0(Desert)에서 A만 이동시켜 B의 음악이 유지되는지 확인한다. 리슨 서버 호스트도 별도 확인한다. 멀티 PIE는 각 플레이어 오디오 디바이스 생성 설정을 켠다.

기존 TDGameUserSettings::GetAudioWorld는 첫 Game/PIE 월드를 선택한다. 단일 프로세스 다중 PIE의 클라이언트별 볼륨 검증에는 제약이 있으며, 이번 변경에서 설정 시스템을 재설계하지 않았다. 분할 화면용 여러 로컬 플레이어의 음악 우선순위 정책도 이번 네트워크 클라이언트 범위에는 포함하지 않는다.
