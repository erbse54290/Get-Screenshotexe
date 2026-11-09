@echo off
setlocal
cd /d "%~dp0"

where zig >nul 2>&1
if errorlevel 1 (
  echo zig wurde nicht gefunden. Download: https://ziglang.org/download/
  exit /b 1
)

if not exist dist mkdir dist

zig cc -target x86_64-windows-gnu -O2 -s "-Wl,--subsystem,windows" src\Get-Screenshot.c -o dist\Get-Screenshot.exe -luser32 -lgdi32 -lshell32 -lole32 -loleaut32
if errorlevel 1 exit /b 1

zig cc -target x86-windows-gnu -O2 -s "-Wl,--subsystem,windows" src\Get-Screenshot.c -o dist\Get-Screenshot-x86.exe -luser32 -lgdi32 -lshell32 -lole32 -loleaut32
if errorlevel 1 exit /b 1

echo Fertig: dist\Get-Screenshot.exe und dist\Get-Screenshot-x86.exe
exit /b 0
