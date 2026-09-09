@echo off
chcp 65001 > nul
setlocal

REM DataTable 검증을 돌린다. 커밋·PR 전에 한 번 실행할 것.
REM
REM 에디터를 띄워 .uasset 을 직접 읽으므로 기동에 1~2분 걸린다.
REM 대신 CSV Export 를 빠뜨려 낡은 데이터로 "통과" 를 찍는 일이 없다.
REM
REM 에디터가 이미 열려 있으면 실패한다. 닫고 실행할 것.

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "UE_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJECT=%~dp0..\TD_Project.uproject"

if not exist "%UE_CMD%" (
    echo 언리얼 에디터를 찾을 수 없습니다.
    echo   %UE_CMD%
    echo 엔진 설치 경로가 다르면 이 파일의 UE_ROOT 를 고쳐주세요.
    pause
    exit /b 1
)

echo 데이터 검증을 시작합니다. 에디터를 여는 중이라 시간이 걸립니다...
echo.

REM -unattended  : 대화 상자를 띄우지 않는다(멈춰 있는 것처럼 보이는 것을 막는다)
REM -nosplash    : 스플래시 창 생략
REM -stdout      : 로그를 이 창에 그대로 찍는다
"%UE_CMD%" "%PROJECT%" -run=pythonscript -script="%~dp0ValidateData.py" -unattended -nosplash -stdout -nopause

if errorlevel 1 (
    echo.
    echo ================================================
    echo   검증 실패. 위의 [오류] 줄을 확인하세요.
    echo ================================================
    pause
    exit /b 1
)

echo.
echo 검증 통과.
pause
