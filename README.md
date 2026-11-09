# Get-Screenshot.exe

Portable Windows-Programm. Macht einen Bildschirmausschnitt und speichert ihn als PNG. Kein PowerShell, kein Setup, kein .NET.

Von Grok erstellt. Stand 09.11.2026.

## Download

Die Programme liegen fertig unter `dist/`:

- `Get-Screenshot.exe` für 64-Bit-Windows
- `Get-Screenshot-x86.exe` für 32-Bit-Windows

Datei herunterladen und aufrufen. Git speichert die EXE bytegenau, Base64 ist nicht nötig.

Neu bauen aus `src/Get-Screenshot.c`: im Projektordner `build.bat`. Dafür wird [Zig](https://ziglang.org/download/) gebraucht.

## Aufruf

```bat
Get-Screenshot.exe -Name HMI -Speicherort C:\Temp -B 1920 -H 1080
```

| Parameter | Bedeutung | Standard |
|---|---|---|
| `-Name` | Anfang des Dateinamens | leer |
| `-Speicherort` | Zielordner | Eigene Dokumente |
| `-OffsetLeft` | Start von links, Pixel | 0 |
| `-OffsetTop` | Start von oben, Pixel | 0 |
| `-B` | Breite | 1920 |
| `-H` | Höhe | 1080 |
| `-?` | Hilfe, kein Screenshot | |

Der Nullpunkt ist die linke obere Ecke des Hauptbildschirms. Ein Monitor links davon hat einen negativen `OffsetLeft`.

`-H` ist die Höhe. Hilfe nur mit `-?` oder `-help`.

Dateiname: `<Speicherort>\<Name>_TTMMJJ_HHMMSS.png`

Beispiel: `C:\Temp\HMI_091126_083415.png`

Die Konsole gibt den vollen Pfad aus. Exitcode 0 heißt gespeichert, 1 falscher Parameter, 2 Bildschirm nicht lesbar, 3 PNG nicht geschrieben.

Muss in der angemeldeten Windows-Sitzung laufen. Als Dienst ohne Desktop gibt es keinen Bildschirm.

Ausführliche Beschreibung: [docs/Funktionsanleitung.md](docs/Funktionsanleitung.md)

Quellcode: [src/Get-Screenshot.c](src/Get-Screenshot.c)
