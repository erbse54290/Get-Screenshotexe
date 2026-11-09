@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
call :join Get-Screenshot.exe.b64
if errorlevel 1 goto fail
call :join Get-Screenshot-x86.exe.b64
if errorlevel 1 goto fail
certutil -decode "%~dp0Get-Screenshot.exe.b64" "%~dp0Get-Screenshot.exe"
if errorlevel 1 goto fail
certutil -decode "%~dp0Get-Screenshot-x86.exe.b64" "%~dp0Get-Screenshot-x86.exe"
if errorlevel 1 goto fail
echo Fertig: Get-Screenshot.exe und Get-Screenshot-x86.exe
goto end
:join
set "OUT=%~1"
del "%OUT%" 2>nul
set "FIRST=1"
for /f "delims=" %%F in ('dir /b /on "%OUT%.part*"') do (
  if "!FIRST!"=="1" (
    copy /b "%%F" "%OUT%" >nul
    set "FIRST=0"
  ) else (
    copy /b "%OUT%"+"%%F" "%OUT%.tmp" >nul
    if errorlevel 1 exit /b 1
    move /y "%OUT%.tmp" "%OUT%" >nul
  )
)
if not exist "%OUT%" exit /b 1
exit /b 0
:fail
echo Zusammenfuegen oder Dekodieren fehlgeschlagen.
exit /b 1
:end
