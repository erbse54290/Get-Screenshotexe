@echo off
setlocal
cd /d "%~dp0"
copy /b "Get-Screenshot.exe.b64.part01"+"Get-Screenshot.exe.b64.part02"+"Get-Screenshot.exe.b64.part03" "Get-Screenshot.exe.b64"
if errorlevel 1 goto fail
copy /b "Get-Screenshot-x86.exe.b64.part01"+"Get-Screenshot-x86.exe.b64.part02"+"Get-Screenshot-x86.exe.b64.part03"+"Get-Screenshot-x86.exe.b64.part04" "Get-Screenshot-x86.exe.b64"
if errorlevel 1 goto fail
certutil -decode "Get-Screenshot.exe.b64" "Get-Screenshot.exe"
if errorlevel 1 goto fail
certutil -decode "Get-Screenshot-x86.exe.b64" "Get-Screenshot-x86.exe"
if errorlevel 1 goto fail
echo Fertig: Get-Screenshot.exe und Get-Screenshot-x86.exe
goto end
:fail
echo Zusammenfuegen oder Dekodieren fehlgeschlagen.
exit /b 1
:end
