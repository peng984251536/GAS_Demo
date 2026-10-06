@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "DamageRoot=%~dp0..\.."
set "DamageOutput=%DamageRoot%\Intermediate\LyraStyleDamageValidation"
if not exist "%DamageOutput%" mkdir "%DamageOutput%"
cl.exe /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /I"%DamageRoot%\Source\GAS_Demo\Public" "%~dp0DamageCalculatorTests.cpp" /Fo"%DamageOutput%\DamageCalculatorTests.obj" /Fe"%DamageOutput%\DamageCalculatorTests.exe"
if errorlevel 1 exit /b 1
"%DamageOutput%\DamageCalculatorTests.exe"
exit /b %errorlevel%
