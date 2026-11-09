decode.bat erwartet die vollstaendigen Dateien Get-Screenshot.exe.b64 und Get-Screenshot-x86.exe.b64.
Die Base64-Daten liegen in Teilen (*.b64.part01, *.b64.part02, ...).
join.bat setzt die Teile zu den .b64-Dateien zusammen und ruft danach certutil auf.
