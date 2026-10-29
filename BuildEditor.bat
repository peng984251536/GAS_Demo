@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Double-click to build the editor. Use --no-pause from a terminal or automation.
rem Optional engine override: set GAS_DEMO_UE_ROOT=D:\GameTools\UE_5.6
set "BUILD_RESULT=1"
set "PROJECT_FILE=%~dp0GAS_Demo.uproject"
set "BUILD_LOG=%~dp0Saved\Logs\BuildEditor.log"
set "ENGINE_ROOT=%GAS_DEMO_UE_ROOT%"
if not defined ENGINE_ROOT set "ENGINE_ROOT=D:\GameTools\UE_5.6"

if not exist "%PROJECT_FILE%" (
    echo ERROR: Project file not found: "%PROJECT_FILE%"
    goto finish
)
if not exist "%ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
    echo ERROR: Unreal Engine build script not found under "%ENGINE_ROOT%".
    echo Set GAS_DEMO_UE_ROOT to the correct engine installation directory.
    goto finish
)
if not exist "%~dp0Saved\Logs" mkdir "%~dp0Saved\Logs"
if not exist "%~dp0Saved\Logs" (
    echo ERROR: Could not create the build log directory.
    goto finish
)

echo Building GAS_DemoEditor Win64 Development...
echo Log: "%BUILD_LOG%"
call "%ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat" GAS_DemoEditor Win64 Development "-Project=%PROJECT_FILE%" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding "-Log=%BUILD_LOG%"
set "BUILD_RESULT=%ERRORLEVEL%"

:finish
echo.
if "%BUILD_RESULT%"=="0" (
    echo BUILD SUCCEEDED
) else (
    echo BUILD FAILED - exit code %BUILD_RESULT%
    echo Check the output above and "%BUILD_LOG%".
)
if /I not "%~1"=="--no-pause" pause
endlocal & exit /b %BUILD_RESULT%
