# Funktionsanleitung Get-Screenshot.exe

Von Grok erstellt. Stand 09.11.2026.

## 1 Zweck

Das Programm fotografiert ein Rechteck des Windows-Desktops und speichert es als PNG. Es ersetzt die PowerShell-Funktion Get-Screenshot auf Rechnern, auf denen PowerShell gesperrt ist.

Eine einzelne EXE. Kein Setup, kein .NET, keine zusätzliche DLL. Windows 10 und Windows 11 reichen.

## 2 Voraussetzungen

- Angemeldete Windows-Sitzung mit sichtbarem Desktop. Als Dienst oder in Sitzung 0 gibt es keinen Bildschirm.
- Aufgabenplanung: „Nur ausführen, wenn der Benutzer angemeldet ist“.
- Zielordner beschreibbar. Fehlt er, legt das Programm ihn an.
- 64-Bit-Windows: `Get-Screenshot.exe`. 32-Bit-Windows: `Get-Screenshot-x86.exe`.

## 3 Aufruf

Ohne Parameter öffnet sich ein Infofenster. Es beschreibt, was das Programm kann und wie der Aufruf aussieht. Dabei wird kein Screenshot erstellt.

Die übrigen Parameter sind optional. Fehlt einer, gilt der Standard: 1920 x 1080 Pixel ab der linken oberen Ecke des Hauptbildschirms, gespeichert unter Eigene Dokumente. Dafür muss mindestens ein Parameter angegeben sein, zum Beispiel nur `-Speicherort`.

```bat
Get-Screenshot.exe -Name HMI -Speicherort C:\Temp -B 1920 -H 1080
```

In der Eingabeaufforderung Anführungszeichen verwenden, wenn Name oder Pfad Leerzeichen enthält. Einfache Anführungszeichen, wie sie PowerShell setzt, werden ebenfalls entfernt.

### Parameter

| Parameter | Bedeutung | Standard |
|---|---|---|
| `-Name` | Anfang des Dateinamens | leer |
| `-Speicherort` | Zielordner der PNG | Eigene Dokumente |
| `-OffsetLeft` | Abstand vom linken Rand, Pixel | 0 |
| `-OffsetTop` | Abstand vom oberen Rand, Pixel | 0 |
| `-B` | Breite des Ausschnitts, Pixel | 1920 |
| `-H` | Höhe des Ausschnitts, Pixel | 1080 |
| `-?` | Hilfe anzeigen, kein Screenshot | |

Der Nullpunkt ist die linke obere Ecke des Hauptbildschirms. Ein Monitor links davon hat einen negativen OffsetLeft, einer darüber einen negativen OffsetTop.

`-H` ist immer die Höhe. Die Hilfe heißt nur `-?` oder `-help`.

### Dateiname

```text
<Speicherort>\<Name>_TTMMJJ_HHMMSS.png
```

Beispiel um 08:34:15 Uhr am 09.11.2026:

```text
C:\Temp\HMI_091126_083415.png
```

- Punkte im Namen werden zu Unterstrichen, ebenso `\ / : * ? " < > |`.
- Ist der Name leer, beginnt die Datei mit einem Unterstrich: `_091126_083415.png`.
- Nach dem Speichern gibt die Konsole den vollen Pfad aus.

## 4 Was intern passiert

Es gibt keinen eigenen Grafiktreiber. Zwei Windows-Schnittstellen, die schon auf dem Rechner liegen: GDI zum Kopieren des Bildschirms und WIC zum Schreiben der PNG.

### 4.1 DPI

Zuerst stellt sich der Prozess auf Per-Monitor-DPI (`PER_MONITOR_AWARE_V2`). Ohne das rechnet Windows bei 150 % Skalierung 1920 x 1080 in einen kleineren Ausschnitt um und streckt ihn. Mit der Einstellung sind `-B` und `-H` echte Bildschirmpixel.

### 4.2 Ausschnitt kopieren

`GetDC(NULL)` öffnet den gesamten Desktop.

1. Eine leere Bitmap in der Größe Breite mal Höhe wird angelegt.
2. `BitBlt` kopiert das Rechteck ab `(OffsetLeft, OffsetTop)` vom Bildschirm in diese Bitmap.
3. Zuerst mit `CAPTUREBLT`, damit auch halbtransparente Fenster mitkommen. Schlägt das fehl, etwa in manchen RDP-Sitzungen, wird nur mit `SRCCOPY` kopiert.
4. `GetDIBits` liest die Pixel als 32 Bit aus: Blau, Grün, Rot, Alpha, erste Zeile oben.

Windows setzt das Alpha-Byte auf 0. Das Programm setzt es auf 255. Sonst wäre die PNG vollständig durchsichtig. Das entspricht der früheren PowerShell-Zeile `Graphics.CopyFromScreen`, nur ohne System.Drawing aus PowerShell.

### 4.3 PNG schreiben

Die Pixel gehen an die Windows Imaging Component (`windowscodecs.dll`). Es wird kein eigener PNG-Schreiber mitgeliefert.

1. WIC-Fabrik über COM öffnen.
2. Bitmap im Speicher anlegen, Format 32bpp BGRA.
3. PNG-Encoder auf die Zieldatei setzen.
4. Frame schreiben, Encoder abschließen, Bitmap und COM freigeben.

## 5 Rückgabe

| Code | Bedeutung |
|---|---|
| 0 | PNG wurde geschrieben, oder es wurde nur das Infofenster bzw. die Hilfe angezeigt. Der Pfad steht in der Konsole, wenn eine Datei entstanden ist. |
| 1 | Parameter falsch oder Zielordner nicht angelegt. |
| 2 | Bildschirm konnte nicht gelesen werden. |
| 3 | PNG konnte nicht geschrieben werden. |

Bei einem Fehler in der Eingabeaufforderung erscheint der Text dort. Beim Doppelklick ohne Konsole erscheint ein Meldungsfenster, aber nur wenn ein Desktop sichtbar ist.

## 6 Beispiele

Doppelklick oder Aufruf ohne Parameter: Infofenster, kein Bild.

Gesamter Hauptbildschirm, Standardgröße:

```bat
Get-Screenshot.exe -Name HMI -Speicherort C:\Temp
```

Fester Ausschnitt:

```bat
Get-Screenshot.exe -Name HMI -Speicherort C:\Temp -OffsetLeft 0 -OffsetTop 0 -B 1920 -H 1080
```

Zweiter Monitor links vom Hauptbildschirm:

```bat
Get-Screenshot.exe -Name Monitor2 -Speicherort C:\Temp -OffsetLeft -1920 -B 1920 -H 1080
```

Hilfe:

```bat
Get-Screenshot.exe -?
```

## 7 Grenzen

- Nur der Desktop der angemeldeten Sitzung.
- Breite und Höhe müssen zwischen 1 und 16384 Pixel liegen.
- Es wird genau ein Rechteck gespeichert, kein automatisches Zusammenfügen aller Monitore.
- Der Zeitstempel ist die lokale Uhr des PCs: Tag, Monat, zweistelliges Jahr, dann Stunde, Minute, Sekunde.
