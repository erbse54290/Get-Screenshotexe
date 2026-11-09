@echo off
certutil -decode "%~dp0Get-Screenshot.exe.b64" "%~dp0Get-Screenshot.exe"
if errorlevel 1 goto fail
certutil -decode "%~dp0Get-Screenshot-x86.exe.b64" "%~dp0Get-Screenshot-x86.exe"
if errorlevel 1 goto fail
echo Fertig: Get-Screenshot.exe und Get-Screenshot-x86.exe
goto end
:fail
echo Dekodieren fehlgeschlagen.
exit /b 1
:end
