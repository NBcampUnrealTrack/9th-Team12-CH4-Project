# TD_PROJECT API

Rust(Axum) + PostgreSQL(SQLx). 프로젝트의 캐릭터별 `FTDPlayerSaveData`를 저장합니다.
언리얼의 기존 더미 로그인/RPC를 HTTP로 교체하는 작업은 별도이며, 이번 폴더는 독립 실행 가능한 API 서버입니다.

## 실행

API 폴더에서:

```powershell
./scripts/setup.ps1
docker compose --env-file .env.api up -d --build
curl.exe http://127.0.0.1:18080/health
```

`setup.ps1`은 기존 `.env`를 건드리지 않고 `.env.api`에 무작위 DB 암호와 게임 서버 키를 만듭니다.
PostgreSQL은 Compose 내부에서만 접근하며 기존 호스트 5432 DB와 충돌하지 않습니다.
PostgreSQL 데이터는 Compose 폴더의 `./sql`을 `/var/lib/postgresql`에 연결해 저장합니다. PostgreSQL 18의 실제 데이터 디렉터리는 `sql/18/docker`입니다. `sql/`은 Git과 Docker 이미지 빌드에서 제외합니다. Compose 컨테이너를 내리거나 재생성해도 이 폴더는 유지되며, 폴더를 직접 삭제하면 DB 데이터가 사라집니다.
API 외부 서비스 시 TLS 프록시를 두고, 키는 게임 **서버**에만 배포하세요. 클라이언트 빌드에 넣지 않습니다.

## 인증·캐릭터 API

| 메서드 | 경로 | 입력/동작 |
|---|---|---|
| GET | `/health` | DB 연결 포함 상태 확인 |
| POST | `/v1/auth/register` | `login_id`, `password` |
| POST | `/v1/auth/login` | `login_id`, `password`; Bearer 토큰 반환 |
| POST | `/v1/auth/logout` | 현재 Bearer 토큰 폐기 |
| GET | `/v1/characters` | 소유 캐릭터 요약 목록, 최대 6개 |
| POST | `/v1/characters` | `character_name`, `class_id` |
| GET | `/v1/characters/{character_id}/save` | `revision`, 전체 `data` 조회 |
| PUT | `/v1/characters/{character_id}/save` | 서버 전용 전체 저장 |

로그인 ID는 영문/숫자/밑줄 2~32자, 대소문자 구분 없이 처리합니다. 신규 비밀번호는 공백을 제외한 영문/숫자/ASCII 특수문자 8~64자입니다.
캐릭터 이름은 문자/숫자/밑줄 2~16자, 이름 중복은 대소문자 구분 없이 전역 검사합니다.
직업은 `Warrior`, `Mage`, `Archer`이며 신규 캐릭터는 레벨 1/경험치 0/골드 0/가방 40칸으로 서버에서 생성합니다.
클라이언트가 초기 골드나 강화 단계를 지정할 수 없습니다.
로그인 토큰 유효기간은 24시간이며 재로그인 시 이전 토큰은 폐기합니다. 만료되면 다시 로그인합니다.
회원가입·로그인은 직접 연결 IP별 분당 30회로 제한합니다. 다중 API 인스턴스/TLS 프록시 환경에서는 프록시에도 공통 제한을 설정하세요.

```json
{"login_id":"player01","password":"your-password"}
```

```json
{"character_name":"새로운전사","class_id":"Warrior"}
```

인증 헤더: `Authorization: Bearer <access_token>`.
저장에는 추가로 `X-Game-Server-Key: <GAME_SERVER_API_KEY>`가 필요합니다.
별도 account_id를 입력받지 않고 인증된 토큰에서 계정을 결정합니다.

## 저장 계약

```json
{
  "request_id": "요청마다 새 UUID",
  "expected_revision": 0,
  "data": "GET save 응답의 data 객체 전체"
}
```

위 `data` 문자열은 설명용 자리 표시자입니다. 실제 요청에는 조회한 JSON 객체를 그대로 넣고 변경된 값을 수정합니다.
처음 조회한 revision이 0이면 저장 성공 후 1입니다. 409 `save_revision_conflict`는 다른 저장이 먼저 반영됐다는 뜻입니다.
다시 조회하고 게임 서버 상태를 조정해야 하며, 과거 스냅샷을 새 revision으로 무조건 재전송하면 안 됩니다.
연결 장애/타임아웃 시에는 **같은 request_id와 같은 본문**으로 재시도하세요. 이미 성공한 요청은 중복 적용하지 않습니다.
다른 본문에 같은 request_id를 쓰면 409 `idempotency_key_reused`입니다.
저장 스냅샷과 요청 영수증은 한 DB 트랜잭션으로 커밋한 뒤 성공 응답합니다.
영수증은 현재 자동 삭제하지 않으며 운영 시 재시도 보장 기간을 정한 후 보존 정책을 추가해야 합니다.

SaveData 필드는 Unreal 원래 이름(PascalCase, `bHasSavedLocation`)입니다. 모든 필드를 필수로 받아 필드 누락에 의한 초기화를 방지합니다.
`FGameplayTag`는 문자열, `FGameplayTagContainer`는 문자열 배열, `FVector`는 `{X,Y,Z}`로 변환합니다.
FJsonObjectConverter의 기본 출력(태그 구조체/lowerCamelCase)과 직접 호환된다고 가정하지 말고 서버 어댑터에서 이 규격으로 변환하세요.
FastArray의 ReplicationID 같은 네트워크 내부 값은 보내지 않습니다.

포함 필드: SaveVersion, ClassId, Level, Exp, SkillLevels, InventorySlotCapacity, InventoryItems, EquippedItems,
HealthRatio, ManaRatio, LastZoneId, LastLocation, bHasSavedLocation, QuestProgressTags, ClaimedChestIds,
ChestClaimRecords, SeenChapterIds, NpcGiftRecords, QuickSlots, Gold, QuestStates, AffectionStates.
장비는 ItemId/SlotIndex/Count/EnhanceLevel/OptionRarity/Options를 모두 보존합니다.
퀵슬롯 Type은 `Empty` 또는 `Item`만 가능합니다. 스킬은 직업별 Q/W/E 경로를 사용합니다.

API는 저장 형식, 범위, 슬롯 중복, 소유권을 검증합니다. 아이템/퀘스트 테이블에 실제로 존재하는지,
경험치와 레벨이 맞는지, 강화/보상이 정당한지는 **권위 있는 UE 게임 서버**가 판정합니다.
세트 효과/최종 스탯은 저장하지 않고 UE가 테이블과 장비로 재계산합니다.
로그아웃 API 자체는 게임 상태를 알 수 없으므로 저장하지 않습니다. UE 서버가 최종 저장 성공 후 로그아웃을 호출해야 합니다.
동일 계정의 동시 접속/월드 점유 정책은 게임 서버에서 관리하며 API 토큰 갱신만으로 기존 UE 연결이 끊기지는 않습니다.

오류: 400 데이터 오류, 401 인증 실패/만료, 403 서버 키 없음, 404 소유 캐릭터 없음,
409 중복/저장 충돌/캐릭터 슬롯 초과, 422 JSON 형식 오류, 429 인증 시도 제한, 500 DB 오류.
업무 오류 본문은 `{"error":{"code":"..."}}`이며 Axum의 JSON/경로 파싱 오류는 기본 오류 응답입니다.

## 확인

```powershell
cargo test --locked
python scripts/smoke.py
```

smoke는 별도 `api_test_...` 계정을 생성하며 기존 데이터를 삭제하지 않습니다.
회원가입/중복/로그인 실패, 계정 격리, 강화·퀘스트 등 전체 저장 왕복, 서버 키 차단,
동일 요청 재시도, 동시 저장 충돌, 동시 캐릭터 생성 6개 제한, 토큰 폐기를 확인합니다.
소스 변경 후에는 `docker compose --env-file .env.api up -d --build`로 새 이미지를 반영합니다.

암호 처리는 [RustCrypto Argon2](https://docs.rs/argon2/0.5.3/argon2/), HTTP/DB 구성은
[Axum SQLx 예제](https://github.com/tokio-rs/axum/blob/main/examples/sqlx-postgres/src/main.rs)를 기준으로 합니다.


## 코드 구성

요청 흐름: `router → controllers → services → PostgreSQL`. 요청·응답과 저장 데이터의 형식은 `models`에서 정의합니다.

| 위치 | 역할 |
|---|---|
| `src/main.rs` | 서버 시작과 종료 |
| `src/app_state.rs` | DB 연결, 마이그레이션, 인증 공유 상태 초기화 |
| `src/router.rs` | API 주소와 컨트롤러 연결 |
| `src/controllers/*_controller.rs` | HTTP 입력 추출, 인증 연결, 상태 코드와 JSON 응답 |
| `src/services/*_service.rs` | 인증, 캐릭터 생성, 저장 규칙과 DB 트랜잭션 |
| `src/models/auth.rs` | 로그인·회원가입 요청과 응답 |
| `src/models/character.rs` | 캐릭터 생성 요청과 목록 응답 |
| `src/models/save.rs` | 저장·조회 요청과 응답 |
| `src/models/save_data.rs` | Unreal SaveData 필드와 유효성 검사 |
| `src/error.rs` | 공통 오류 응답 |

예를 들어 저장 기능은 `save_controller.rs → save_service.rs` 순서로 동작합니다.
컨트롤러에서는 SQL을 실행하지 않으며, 서비스는 HTTP 헤더나 Axum 추출기를 받지 않습니다.
DB 접근은 서비스에 두어 소유권 확인·버전 충돌 확인·저장 영수증 기록을 하나의 트랜잭션으로 처리합니다.


## 캐릭터 삭제

`DELETE /v1/characters/{character_id}` — `Authorization: Bearer <access_token>` 필요, 요청 본문 없음.

- 본인 소유 캐릭터 삭제 성공: `204 No Content`.
- 인증 누락·만료: `401`. 다른 계정의 캐릭터·없는 캐릭터·이미 삭제된 캐릭터: `404 character_not_found`.
- 영구 삭제이며 캐릭터의 SaveData와 저장 요청 영수증도 함께 삭제됩니다.
- 삭제한 슬롯과 캐릭터 이름은 다시 사용할 수 있습니다. 새 캐릭터는 새로운 UUID와 초기 데이터로 생성됩니다.
- 생성과 삭제는 같은 계정 잠금을 사용합니다. 삭제 이후 도착하는 기존 캐릭터 저장 요청은 `404`이며 데이터를 복원하지 않습니다.
- 현재 API에는 게임 접속 중인 캐릭터를 추적하는 기능이 없습니다. 게임 연동 시 캐릭터 선택 화면에서만 삭제를 허용하고, 플레이 중 삭제 방지는 게임 서버의 접속 상태와 연결해야 합니다.
