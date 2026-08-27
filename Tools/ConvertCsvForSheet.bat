@echo off
chcp 65001 > nul
setlocal

REM 언리얼이 내보낸 DataTable CSV 를 구글 시트용으로 변환한다.
REM 폴더를 이 배치 파일 위로 끌어다 놓거나, 그냥 더블클릭하고 경로를 입력하면 된다.
REM
REM 파이썬을 따로 설치하지 않아도 되도록 언리얼에 딸려온 것을 쓴다.

set "UE_PYTHON=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"

if not exist "%UE_PYTHON%" (
    echo 언리얼 파이썬을 찾을 수 없습니다.
    echo   %UE_PYTHON%
    echo 엔진 설치 경로가 다르면 이 파일의 UE_PYTHON 을 고쳐주세요.
    pause
    exit /b 1
)

set "PYTHONIOENCODING=utf-8"

if "%~1"=="" (
    "%UE_PYTHON%" "%~dp0ConvertExportedCsv.py"
) else (
    "%UE_PYTHON%" "%~dp0ConvertExportedCsv.py" "%~1"
)

echo.
pause
