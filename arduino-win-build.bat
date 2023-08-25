@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
setlocal
REM go to the folder where this bat script is located
cd /d %~dp0

set /a EXPECTED_CLI_MAJOR=0
set /a EXPECTED_CLI_MINOR=18

set ARDUINO_CORE=arduino:mbed_portenta
set BOARD=arduino:mbed_portenta:envie_m7:split=100_0
set ARDUINO_CLI=arduino-cli
set BUILD_OPTION=--build
set FLASH_OPTION=--flash
set ALL_OPTION=--all

IF [%1]==[] (
    GOTO NOPARAMETER
)

set COMMAND=%1

FOR %%I IN (.) DO SET DIRECTORY_NAME=%%~nI%%~xI

where /q arduino-cli
IF ERRORLEVEL 1 (
    GOTO NOTINPATHERROR
)

REM parse arduino-cli version
FOR /F "tokens=1-3 delims==." %%I IN ('arduino-cli version') DO (
    FOR /F "tokens=1-3 delims== " %%X IN ('echo %%I') DO (
        set /A CLI_MAJOR=%%Z
    )
    SET /A CLI_MINOR=%%J
    FOR /F "tokens=1-3 delims== " %%X IN ('echo %%K') DO (
        set /A CLI_REV=%%X
    )
)

if !CLI_MINOR! LSS !EXPECTED_CLI_MINOR! (
    GOTO UPGRADECLI
)

if !CLI_MAJOR! NEQ !EXPECTED_CLI_MAJOR! (
    echo You're using an untested version of Arduino CLI, this might cause issues (found: %CLI_MAJOR%.%CLI_MINOR%.%CLI_REV%, expected: %EXPECTED_CLI_MAJOR%.%EXPECTED_CLI_MINOR%.x )
) else (
    if !CLI_MINOR! NEQ !EXPECTED_CLI_MINOR! (
        echo You're using an untested version of Arduino CLI, this might cause issues (found: %CLI_MAJOR%.%CLI_MINOR%.%CLI_REV%, expected: %EXPECTED_CLI_MAJOR%.%EXPECTED_CLI_MINOR%.x )
    )
)

echo Finding Arduino Mbed core...

(arduino-cli core list  2> nul) | findstr "arduino:mbed_portenta"
IF %ERRORLEVEL% NEQ 0 (
    GOTO INSTALLMBEDCORE
)

:AFTERINSTALLMBEDCORE

:: define and include
set DEFINE=-DARDUINOSTL_M_H -DMBED_HEAP_STATS_ENABLED=1 -DMBED_STACK_STATS_ENABLED=1 -O3 -DEI_SENSOR_AQ_STREAM=FILE -DEIDSP_QUANTIZE_FILTERBANK=0 -DEI_CLASSIFIER_SLICES_PER_MODEL_WINDOW=4 -DEI_DSP_IMAGE_BUFFER_STATIC_SIZE=128 -DEI_CAMERA_FRAME_BUFFER_SDRAM -DEI_CLASSIFIER_ALLOCATION_STATIC -w
set INCLUDE=-I.\\src\\ -I.\\src\\model-parameters\\ -I.\\src\\ingestion-sdk-c\\ -I.\\src\\ingestion-sdk-c\\inc\\signing\\ -I.\\src\\ingestion-sdk-platform\\portenta-h7\\ -I.\\src\\sensors\\ -I.\\src\\sensors\\ -I.\\src\\QCBOR\\inc\\ -I.\\src\\QCBOR\\src\\ -I.\\src\\mbedtls_hmac_sha256_sw\\ -I.\\src\\edge-impulse-sdk\\ -I.\\src\\firmware-sdk\\

rem CLI v0.14 updates the name of this to --build-property
set BUILD_PROPERTIES_FLAG=--build-property

:: just build
IF %COMMAND% == %BUILD_OPTION% goto :BUILD

echo Finding Arduino Mbed core OK

echo Finding Arduino Portenta H7...

set COM_PORT=""

for /f "tokens=1" %%i in ('arduino-cli board list ^| findstr "Arduino Portenta H7"') do (
    set COM_PORT=%%i
)

IF %COM_PORT% == "" (
    GOTO NOTCONNECTED
)

echo Finding Arduino Portenta H7 OK at %COM_PORT%

IF %COMMAND% == %FLASH_OPTION% goto :FLASH

IF %COMMAND% == %ALL_OPTION% goto :ALL else goto :COMMON_EXIT

echo No valid command

goto :COMMON_EXIT

:BUILD
    echo Building %PROJECT%
    %ARDUINO_CLI% compile --fqbn %BOARD% %BUILD_PROPERTIES_FLAG% "build.extra_flags=%DEFINE% %INCLUDE%" --output-dir .
goto :COMMON_EXIT

:FLASH
    echo Flashing %PROJECT%
    CALL %ARDUINO_CLI% upload -p %COM_PORT% --fqbn %BOARD%  --input-dir .
goto :COMMON_EXIT

:ALL
    echo Building %PROJECT%
    %ARDUINO_CLI% compile --fqbn %BOARD% %BUILD_PROPERTIES_FLAG% "build.extra_flags=%DEFINE% %INCLUDE%" --output-dir .
    echo Flashing %PROJECT%
    CALL %ARDUINO_CLI% upload -p %COM_PORT% --fqbn %BOARD%  --input-dir .
goto :COMMON_EXIT


IF %ERRORLEVEL% NEQ 0 (
    GOTO FLASHINGFAILEDERROR
)

echo Flashed your Arduino Portenta H7 development board
echo To set up your development with Edge Impulse, run 'edge-impulse-daemon'
echo To run your impulse on your development board, run 'edge-impulse-run-impulse'

@pause
exit /b 0

:NOTINPATHERROR
echo Cannot find 'arduino-cli' in your PATH. Install the Arduino CLI before you continue
echo Installation instructions: https://arduino.github.io/arduino-cli/latest/
@pause
exit /b 1

:INSTALLMBEDCORE
echo Installing Arduino Mbed core...
arduino-cli core update-index
arduino-cli core install arduino:mbed_portenta@2.8.0
echo Installing Arduino Mbed core OK
GOTO AFTERINSTALLMBEDCORE

:NOTCONNECTED
echo Cannot find a connected Arduino Portenta H7 development board via 'arduino-cli board list'
echo If your board is connected, double-tap on the RESET button to bring the board in recovery mode
@pause
exit /b 1

:UPGRADECLI
echo You need to upgrade your Arduino CLI version (now: %CLI_MAJOR%.%CLI_MINOR%.%CLI_REV%, but required: %EXPECTED_CLI_MAJOR%.%EXPECTED_CLI_MINOR%.x or higher)
echo See https://arduino.github.io/arduino-cli/installation/ for upgrade instructions
@pause
exit /b 1

:FLASHINGFAILEDERROR
echo Flashing failed. Here are some options:
echo If your error is 'incorrect FQBN' you'll need to upgrade the Arduino core via:
echo      $ arduino-cli core update-index
echo      $ arduino-cli core install arduino:mbed_portenta@2.8.0
echo Otherwise, double tap the RESET button to load the bootloader and try again
@pause
exit /b %ERRORLEVEL%

:NOPARAMETER
echo No arguments, pleaase invoke with --build or --flash or --all. See README.md for instructions
exit /b 1

:COMMON_EXIT