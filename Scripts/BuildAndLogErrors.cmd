@echo off
REM 编译 GAS_DemoEditor 并把报错单独提取出来，方便快速定位。
REM
REM 用法：双击运行，或在命令行执行
REM     Scripts\BuildAndLogErrors.cmd
REM
REM 产出：
REM     Saved\BuildLogs\Build.log        完整构建日志
REM     Saved\BuildLogs\BuildErrors.txt  只含 error / warning 行，通常几行到几十行
REM
REM 如果只是想让 AI 帮忙看报错，把 BuildErrors.txt 的内容贴出去即可。

setlocal enabledelayedexpansion

set "EngineRoot=D:\GameTools\UE_5.6"
set "ProjectRoot=%~dp0.."
set "ProjectFile=%ProjectRoot%\GAS_Demo.uproject"
set "LogDir=%ProjectRoot%\Saved\BuildLogs"
set "FullLog=%LogDir%\Build.log"
set "ErrorLog=%LogDir%\BuildErrors.txt"

if not exist "%LogDir%" mkdir "%LogDir%"

if not exist "%EngineRoot%\Engine\Build\BatchFiles\Build.bat" (
    echo [错误] 找不到引擎构建脚本：%EngineRoot%\Engine\Build\BatchFiles\Build.bat
    echo        如果你的引擎装在别处，改这个文件顶部的 EngineRoot。
    exit /b 1
)

echo 正在编译，输出到 %FullLog%
echo 首次编译或改动头文件时可能需要几分钟。

call "%EngineRoot%\Engine\Build\BatchFiles\Build.bat" ^
    GAS_DemoEditor Win64 Development ^
    -Project="%ProjectFile%" ^
    -WaitMutex -FromMsBuild ^
    > "%FullLog%" 2>&1

set "BuildResult=%errorlevel%"

REM 提取报错行。UBT 输出里错误行的格式是：
REM     <路径>(<行>,<列>): error C1234: <说明>
REM 用 findstr 取，顺便把紧随其后的源码行也保留下来便于定位。
findstr /R /C:"error [A-Z][0-9]*" /C:"error :" /C:"Error:" "%FullLog%" > "%ErrorLog%"
if errorlevel 1 (
    REM findstr 没匹配到会返回 1，说明没有编译错误。
    > "%ErrorLog%" echo 没有编译错误。
)

echo.
echo ============================================================
if "%BuildResult%"=="0" (
    echo 编译成功。
) else (
    echo 编译失败，错误码 %BuildResult%
    echo.
    echo ---- 报错摘要 ----
    type "%ErrorLog%"
    echo ------------------
)
echo.
echo 完整日志：%FullLog%
echo 报错摘要：%ErrorLog%
echo ============================================================

exit /b %BuildResult%
