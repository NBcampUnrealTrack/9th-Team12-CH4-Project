# 언리얼 저장 연동

## 실행

기존 로그인 입력 규칙은 변경하지 않는다. 에디터는 Play 시작 시 `API/.env`를 읽는다.
`TD_SAVE_MODE=dummy`는 환경변수에 관계없이 기존 메모리 더미 저장소를 사용하고,
`TD_SAVE_MODE=backend`는 환경변수의 API 설정을 우선 사용하며 없으면 `.env`의
`API_PORT`(기본 18080), `GAME_SERVER_API_KEY`로 로컬 API에 연결한다.
모드를 생략하면 backend이며, 잘못된 값은 저장 요청을 차단한다. 값을 바꾼 뒤 Play를 종료하고 다시 시작한다.
더미와 DB 데이터는 자동 이동하지 않는다. `.env` 파일도 환경변수도 없으면 더미 모드다.
이 자동 로딩과 모드 선택은 WITH_EDITOR 전용이며, 패키징 빌드는 기존 서버 환경변수를 사용한다.

API 폴더에서 최신 API와 새 마이그레이션을 반영한 뒤, 아래 실행기를 사용한다.

```powershell
docker compose --env-file .env up -d --build
./scripts/start-unreal-backend.ps1 -Editor
```

`-Editor`를 빼면 에디터 실행 파일의 dedicated-server 모드로 GameTestLevel을 실행한다. 클라이언트는 기존 접속 흐름을 사용한다.
실행기는 `.env`의 서버 키와 API 포트를 새 UE 프로세스 환경에만 전달한다. 기존에 열린 에디터에는 적용되지 않는다.
일반 에디터 실행은 이 실행기 없이도 위 `.env` 설정을 읽는다. 더미 테스트는 API를 검사하는 실행기 대신 에디터를 직접 실행한다.
별도 호스트의 게임 서버는 `TD_BACKEND_URL=https://...`, `TD_GAME_SERVER_KEY=...`를 서버 프로세스에 설정한다.
서버 키를 클라이언트 패키지, Data Asset, 공개 설정 파일에 넣지 않는다. HTTP 평문은 localhost 연결만 허용한다.

## 데이터 흐름

1. 로그인 성공 후 API의 계정 UUID와 토큰을 서버에서 보관한다.
2. 목록 배열의 선택 항목에서 캐릭터 UUID를 얻는다. DB `slot_index`를 배열 인덱스로 사용하지 않는다.
3. 캐릭터 사용권 획득 → 데이터 조회 → 엄격한 JSON 변환 → 컴포넌트 복원 → 기존 GameMode 스폰 순서로 처리한다.
4. `TDSave` 또는 서버 `SaveCharacter()` 호출로 저장한다. 백엔드 모드에서 bool 성공은 요청 접수 의미이며 DB 완료는 비동기 통지된다.
5. 캐릭터별 저장은 하나씩 실행한다. 저장 중 변경된 최신 상태를 응답 후 다시 수집해 후속 요청으로 저장한다.
6. 캐릭터 선택 화면 복귀·로그아웃은 최종 저장 확인 후 기존 Pawn/PlayerState를 정리하고 사용권을 해제한다.
7. 연결 종료는 Controller `Destroyed()`에서 Pawn 제거 전에 스냅샷을 수집한다. 서버가 살아 있는 동안 비동기 저장을 마무리한다.

별도의 주기적 게임 상태 자동 저장은 추가하지 않았다. 30초 타이머는 사용권 갱신과 실패 요청 재시도에 사용한다.

## 충돌과 복구

- 서버 사용권은 120초이며 30초 간격으로 갱신한다. 계정당 한 캐릭터만 점유한다.
- `/v1/characters/{id}/lease`: POST 획득, PUT 갱신, DELETE 해제. 서버 키와 64자리 난수 `X-Character-Lease`가 필요하다. 획득에는 사용자 Bearer 토큰도 필요하다.
- 활성 사용권의 저장/조회는 서버 키와 사용권 토큰으로 인증한다. 사용자 토큰 만료·재로그인이 기존 서버의 최종 저장을 막지 않는다.
- 사용 중인 캐릭터의 일반 토큰 저장 및 삭제는 API에서도 `409 character_in_use`로 차단한다.
- 만료된 사용권의 저장은 거부한다. 이전 소유자가 새 소유자의 사용권을 해제할 수 없다.
- 저장 충돌을 새 revision으로 강제 덮어쓰지 않는다. 충돌·사용권 상실 시 플레이를 종료하고 재로그인할 수 있게 한다.
- 네트워크/5xx 재시도는 같은 요청 UUID와 동일 본문을 최대 3회 전송한다. 응답의 캐릭터·요청 UUID·revision을 확인한 뒤 다음 저장으로 넘어간다.
- 요청 전 `Saved/BackendOutbox/<character UUID>-<recovery UUID>.json`에 진행 중 요청과 최신 스냅샷을 임시 파일 교체 방식으로 보관한다. 비밀 키나 인증 토큰은 포함하지 않는다. DB 반영 확인 후 파일을 제거한다. 새 접속은 별도의 복구 ID를 사용하므로 과거 충돌 파일을 덮어쓰지 않는다.
- 충돌 파일은 운영자가 DB revision 및 요청 영수증과 비교해 복구한다. 재시작 시 자동 덮어쓰기는 하지 않는다. 디스크 장애·프로세스 강제 종료 직전의 미수집 변경까지 보장하지 않는다.
- 월드/서버 전체 종료 시에는 HTTP 대기 시간이 보장되지 않는다. 정상 운영 종료는 플레이어 최종 저장 및 사용권 해제 완료 후 실행한다. 별도의 서버 종료 명령 자동화는 이 변경에 포함하지 않는다.

## 검증

- `cargo test --locked`: 백엔드 단위 테스트.
- `TD.Save.Backend.Codec`: UE 구조체 전체 왕복, 필드 누락/알 수 없는 필드/태그 거부, FastArray 내부 필드 제외.
- `scripts/integration.py`: UE JSON → 격리 PostgreSQL → 동일 JSON, 동시 저장, 재시도, 토큰 폐기, 사용권 만료, 삭제 제한, 빈 슬롯 확인.
- `TD.Save.Backend.HttpQueue`: 실제 UE HTTP 저장 중 최신 스냅샷을 추가해 두 revision으로 저장 및 해제 확인.
- `TD.Save.Backend.Import`: PostgreSQL에서 반환한 파일을 UE SaveData로 복원.
- `TD.Save.Backend.ControllerFlow`: 서버 월드에서 실제 PlayerController의 로그인·생성·삭제·선택·저장·선택 화면 복귀·연결 종료·재접속·로그아웃 확인.

HTTP 테스트는 `-TDBackendIntegration`을 지정한 경우에만 `127.0.0.1:18081`의 격리 API를 사용한다. 테스트용 컨테이너 이름은 `td-save-integration-postgres`, DB 포트는 `18082`이다. 기존 API/DB에는 테스트 데이터를 쓰지 않는다.

격리 API 실행 순서: `cargo build --locked` → `scripts/start-integration.ps1` → UE Codec 실행으로 `Saved/Automation/backend-codec.json` 생성 → `python scripts/integration.py` → 나머지 UE 테스트 실행.
테스트 후 실행기가 출력한 테스트 API PID를 종료하고 `docker stop td-save-integration-postgres`로 일회용 DB를 정리한다. API 로그와 UE 로그/JSON은 `Saved/Automation`에 남는다.
