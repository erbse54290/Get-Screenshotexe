# Get-Screenshot.exe

Portable Windows-Programm. Macht einen Bildschirmausschnitt und speichert ihn als PNG. Kein PowerShell, kein Setup, kein .NET.

Von Grok erstellt. Stand 09.11.2026.

## Download

Die EXE-Dateien liegen unter `dist/` als Base64, weil der Upload rohe Binärdateien verfälschen würde. Auf dem Windows-PC im Ordner `dist`:

```bat
decode.bat
```

Das erzeugt `Get-Screenshot.exe` (64-Bit) und `Get-Screenshot-x86.exe` (32-Bit). `certutil` gehört zu Windows, PowerShell ist nicht nötig.

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
