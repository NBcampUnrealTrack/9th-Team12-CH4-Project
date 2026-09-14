@echo off
chcp 65001 > nul
setlocal

REM DataTable 을 CSV 로 뽑고, 이어서 사람이 읽는 형태로 정리한다.
REM
REM 에디터를 띄워 .uasset 을 직접 읽으므로 기동에 1~2분 걸린다.
REM 에디터가 이미 열려 있으면 실패한다. 닫고 실행할 것.
REM
REM 결과는 두 곳에 생긴다.
REM   Content\Data\DataTables\<도메인>\CSV\             언리얼이 뱉은 원본
REM   Content\Data\DataTables\<도메인>\CSV\GoogleSheet\ 정리본 (이쪽을 보면 된다)

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "UE_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "UE_PYTHON=%UE_ROOT%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
set "PROJECT=%~dp0..\TD_Project.uproject"
set "TABLES=%~dp0..\Content\Data\DataTables"

if not exist "%UE_CMD%" (
    echo 언리얼 에디터를 찾을 수 없습니다.
    echo   %UE_CMD%
    echo 엔진 설치 경로가 다르면 이 파일의 UE_ROOT 를 고쳐주세요.
    pause
    exit /b 1
)

echo 테이블을 내보냅니다. 에디터를 여는 중이라 시간이 걸립니다...
echo.

"%UE_CMD%" "%PROJECT%" -run=pythonscript -script="%~dp0ExportTables.py" -unattended -nosplash -stdout -nopause

if errorlevel 1 (
    echo.
    echo 내보내기 실패. 위의 로그를 확인하세요.
    pause
    exit /b 1
)

echo.
echo 사람이 읽는 형태로 정리합니다...
echo.

set "PYTHONIOENCODING=utf-8"

"%UE_PYTHON%" "%~dp0ConvertExportedCsv.py" "%TABLES%\Character\CSV"
"%UE_PYTHON%" "%~dp0ConvertExportedCsv.py" "%TABLES%\Item\CSV"

echo.
echo 완료.
pause
