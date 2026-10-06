@echo off
setlocal
set "SOURCE=%~dp0bin\Release\net9.0-windows"
set "TARGET=D:\GameTool\AutoClicker\bin\Release\net9.0-windows"

if not exist "%SOURCE%\AutoClicker.exe" (
  echo 未找到已编译的 AutoClicker.exe。
  pause
  exit /b 1
)

robocopy "%SOURCE%" "%TARGET%" /E /NFL /NDL /NJH /NJS
if errorlevel 8 (
  echo 更新失败。请关闭正在运行的 AutoClicker 后重试。
  pause
  exit /b 1
)

echo 更新完成。请运行：
echo %TARGET%\AutoClicker.exe
pause
