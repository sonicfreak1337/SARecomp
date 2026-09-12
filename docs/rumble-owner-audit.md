# PAL rumble owner audit

Stand: 2026-09-12. Begrenzte statische Prüfung für den Sonic-Port. Keine
Builds, Spielstarts, Hardwareabfragen oder Shared-Source-Änderungen. Der alte
Katana-Tree wurde ausschließlich als vorhandene Binär-/Disasmquelle gelesen.
Nach dem unten geschlossenen Titelcaller/SDK-Owner wurde die Suche beendet.

Nachtrag: Der spätere Flycast-Abgleich im letzten Teil dieses Berichts schließt
die dort ursprünglich offene Zeitsemantik. Eggman hat die belegte Bridge
anschließend integriert; siehe [Umsetzung und Tests](options-input-audio-completion.md).
Die erste Auditfassung unten bleibt zur Trennung von Befund und Nachtrag erhalten.

## Ergebnis

Die fehlende PuruPuru-Capability ist ein **konkreter früher Abbruchgrund**:
Der Titel prüft sie, bevor er seinen Vibrationsrequesttask erzeugt. Die native
Inputprojektion bietet sie nicht an. Im gebundenen postPAL-Snapshot besitzt
keiner der 24 SDK-Geräteslots das geprüfte Bit. Das erklärt die fehlenden
Requests auf diesem nachgewiesenen Pfad, ohne Gameplay-Hit-Heuristik.

Eine kleine portlokale Bridge hat belegte Capability-/Start-/Stop-Grenzen
und ein bytegenaues Requestformat. Der anschließende, ausdrücklich beauftragte
Flycast-Abgleich unten schließt auch Timeout und Hostabbildung. Damit ist eine
native Bridge für den nachgewiesenen Titelcaller implementierbar. Die
Hostabbildung entspricht einer benannten primären Implementierung; sie ist
kein neuer Nachweis physikalischer Gleichheit mit dem Originalmotor.

## Quellen und Bindung

Die Quellen liegen unter
`C:/Users/ultim/Desktop/KatanaRecomp/private/`:

- `native-content/Sonic Adventure PAL v1.003/postpal-main-ram.bin`,
  16,777,216 Bytes, Basis `8C000000`, SHA-256
  `3704eb66597dc1a3c2e66d48fd050e0924c97ff2b956774aa4e2700319c0c80a`.
  Entspricht `postpal_title_ram_binding` in `src/native_title_adapter.cpp`.
- Vorhandene Disassemblierungen:
  `analysis/sonic-adventure-pal-v1003/r354-primary-coverage-v3/primary-00000000.disasm.txt`
  und `primary-00400000.disasm.txt`. Ihre Quellbinärdatei heißt
  `postpal-main-ram-native-ready.bin`. Alle unten gehashten Codeintervalle
  wurden direkt gegen die gebundene Originaldatei verglichen: bytegleich.
- `pdVib Ver 1.01 Build:Mar 02 1999 21:20:29` steht bei `8C66D52D`.
  Die Versionszeichenfolge allein war kein Ownerbeweis; der folgende Caller
  und seine vollständige Capability-/Requestkette sind der Beweis.

SHA-256 über halb offene Intervalle `[Start, Ende)`:

| Intervall | SHA-256 |
| --- | --- |
| `8C109D60..8C109DEC` | `709e4fd47f3e72b9a15c39c2fd1590e18239f6df2c07fb6dd72cf67959681e32` |
| `8C109DEC..8C109F1E` | `16633f5614110fb0ae22b0bdc340edfa71fb9900945d5d81bd72f0980a62197f` |
| `8C6042B0..8C604384` | `0d774d826295bb7986cf701d93d870758bed57b75e3a0d4d754f71f2a5d3123d` |
| `8C64BE18..8C64BE3E` | `3218b6cbe1cf9df61daca868a3ddd49d12077a6c3d6c874a70b9f41b4f3a838d` |
| `8C64BEF2..8C64BFDC` | `314cc41fbb2a2d29c6c462c8448f991e033c41ee03f4eb450863818ee2efc9b0` |
| `8C65F2E0..8C65F2F2` | `9b5c70c5db4bf45d110e60ddb7a931692652b6e88d85fac431fc2b7ddd08fecc` |

## Ein geschlossener Titelcaller

`8C109DEC` erhält `r4=Spielerindex`, `r5=signierter Stärkeparameter`,
`r6=zweiter numerischer Parameter`, `r7=Timeoutparameter`.
Die folgenden Bezeichnungen beschreiben beobachtete Operationen; sie sind
keine aus unbekannten SDK-Headern übernommenen Symbolnamen.

1. `8C109DFE..8C109E0E`: Nichtnull bei `8C1BFD60` bricht ab.
2. `8C109E10..8C109E20`: `8C754DDC[player]` wählt einen Controllerport;
   `-1` bedeutet keinen Port.
3. `8C109E22..8C109E4A`: Über `8C15FF14[2*port + 0/1]` werden bis zu
   zwei Zubehörslots auf `8C6042B0` geprüft. Die Tabelle enthält im Snapshot
   `{1,2,7,8,13,14,19,20}`. Beide Abfragen false führen bei
   `8C109E4C..8C109E50` zu `8C109F0A`, **vor jeder Taskerzeugung**.
4. Stärke wird auf `[-4,4]` begrenzt; `+1/-1` wird `+2/-2`.
   Der zweite Parameter wird auf `[7,59]`, Timeout auf `[0,255]` begrenzt.
5. Timeout != 0: `8C109EF6` ruft `8C6042B6(slot, timeout)` und setzt das
   Requestflag auf 1; sonst ist es 0. Der Konfigurationsrückgabewert wird hier
   nicht als Erfolgstest ausgewertet.
6. `8C109F04` ruft `8C109D90(slot, flag, strength, secondParameter, 0)`.
   Der fünfte Parameter liegt am eingehenden Stack. Der Konstruktor erzeugt
   den originalen Task mit Callback `8C109D60`, reserviert 8 Requestbytes
   und schreibt Bytes 0..4: `{1, flag, strength, secondParameter, 0}`.
7. `8C109D60` liest Task+44 als Workpointer und Task+40 als Requestpointer.
   Work+4 wird inkrementiert; nur wenn der vorherige Wert 1 war, ruft
   `8C109D7C` `8C60431A` mit `r4=work[0]` und `r5=request` auf und
   übergibt danach an `8C09859C` zur Taskbeendigung. Der ursprüngliche
   Taskzeitpunkt soll erhalten bleiben; keine Bindung an Render-FPS.

Ein zweiter benachbarter Titelcaller `8C109F1E` hat dieselbe frühe
Capabilitysuche bei `8C109F5A..8C109F7C`. Er wurde nicht weiter verfolgt.
Keine Aussage, welche konkreten Treffer-/Gameplayereignisse diese Funktionen
aufrufen; dafür wäre eine weitere, hier absichtlich unterlassene Suche nötig.

## Kleinste SDK-Grenzen und ABI

| Owner | Eingabe | Beobachteter Vertrag |
| --- | --- | --- |
| `8C6042B0` | `r4=Zubehörslot` | Tailcall `8C64BE18`; `r0=0/1` Capability |
| `8C6042B6` | `r4=Slot, r5=Konfigurationsbyte` | Capabilitytest; übermittelt `{0,2,low8(r5),0}` an Geräteservice `8C646934` für Funktion `0x100` |
| `8C60431A` | `r4=Slot, r5=Requestpointer` | Capabilitytest; Tailcall `8C64BEF2` mit `r6=1` |
| `8C604348` | `r4=Slot` | Derselbe Requestowner mit `r5=8C66CA48`, `r6=1`; Konstante `01 00 00 00 00 00 00 00` (Nullamplitude/Stop) |
| `8C64BEF2` | `r4=Slot, r5=Array, r6=Anzahl` | 8-Byte-Records in 4-Byte-Commands umpacken; `8C6469E0(slot,0x100,packed,count)` |

`8C60431A`, `8C604348` und `8C64BEF2` liefern `r0=-2` ohne Capability,
`-1` bei negativem Geräteserviceergebnis, sonst `0`. `8C6042B6` verwendet
dieselbe beobachtete Dreiteilung. Der Titel wertet den Startreturn im
geprüften Callback nicht aus. SDK-Stop ist als Funktion/Request vorhanden;
kein direkter Titelcaller auf `8C604348` wurde im residentes Snapshot als
32-Bit-Pointer gefunden. Das ist kein Nachweis sämtlicher Overlaycaller.

Requestpacking in `8C64BF34..8C64BFAC`, Eingang `b[0..7]`:

- `out[0]=(b[0]<<4)|(b[1]&1)` (Byteergebnis).
- Signierter Wert `b[2]`: Wenn `(b[1]&0x88)==0`, werden +1/-1 zu +2/-2.
  Positive Werte bilden `(value<<4)&0x70`, negative `(-value)&7`, null 0.
  Dazu OR mit `b[1]&0x88` für `out[1]`.
- `out[2]=b[3]`.
- `out[3]=b[4]`, wenn `b[1]&0x88`, sonst 0.
- Der Owner springt zum nächsten Eingang um 8 Bytes; Bytes 5..7 werden
  in diesem Packing nicht gelesen.

Damit sind Amplitudennull und das genaue Packing belegt. Welche Hostmotoren
eine signierte Stärke bedienen sollen, Frequenzabbildung und Timeoutzeit
sind native Backendentscheidungen mit noch offener Originalsemantik.

## Warum Capability derzeit fehlt

`8C64BE18` ruft `8C6467A8(slot)` -> `8C65F2E0(slot)` und prüft
`*(uint32_t*)(*(uint32_t*)8C8A82F4 + 0x44C + 120*slot) & 0x100`.
Dies sind **120-Byte-Gerätedeskriptoren**, nicht die 52-Byte-Inputrecords.

Im gebundenen Snapshot ist `*8C8A82F4=8C8A71F8`. Die 24 Functionwords
sind Slot 0 = 1, Slot 1 = 2, alle anderen = 0; keines enthält `0x100`.
`8C754DDC` enthält `{0,-1,-1,-1}`. Für den ersten Spieler prüft der
Caller somit Slots 1 und 2: beide false.

`service_sonic_native_input()` publiziert Controllerbuttons/Achsen in
`8C88E8E4` mit 52-Byte-Stride und spiegelt Titelrecords. Es aktualisiert
diese Capabilitydeskriptoren nicht. Der Framebeginpfad dokumentiert explizit,
dass `8C641E40` und dessen vollständiger Geräteunterbaum ausgelassen werden.
Eine Anfrage an vorhandene `set_gamepad_vibration()` entsteht dort nicht.
Das ist ein statischer Pfadbeweis, kein neuer Hardware-/Laufzeitmitschnitt
und keine Behauptung, sämtliche denkbaren Guestwrites geprüft zu haben.

## Kleinster implementierbarer nächster Schritt

Der unmittelbar implementierbare Teil ist eine bytegebundene native
Request-/Capabilitygrenze, zunächst mit prüfbaren synthetischen Fixtures:

1. `8C6042B0` beantwortet nur die belegten Zubehörslots eines aktuell
   gebundenen Hostcontrollers mit nutzbarem Rumblebackend. Slotauflösung aus
   Titelzuordnung und obiger Originaltabelle; keine Annahme `Slot=P1`.
   Den 52-Byte-Buttonidentitywert zu erweitern hätte hier keinen Effekt.
2. `8C6042B6`, `8C60431A`, `8C604348` vollständig am SDK-Eintritt
   übernehmen, sodass keine Maple-Nachfahren laufen. Konfigurationsbyte und
   Requestbytes lokal pro Identität/Generation halten, Nullamplitude stoppen,
   Rückgabesemantik erhalten. Für die erste gültige Umsetzung muss die
   Zeitsemantik noch geschlossen sein, **bevor** Capability im Produkt true
   meldet. Capability-only würde Originalrequests in den Gerätepfad freigeben.
3. Danach auf der vorhandenen Plattform `set_gamepad_vibration()` ausgeben;
   bestehende Stärkeskalierung nur einmal anwenden. Die aktuelle Plattform
   unterstützt XInput und korrelierte Sony->XInput-Aliase; ein direktes Sony-
   Backend bleibt eine eigene Arbeit. Der Rumbleowner darf keine nicht
   ausgebbare Capability versprechen.
4. Native Stops bei Menü-/Pauseeintritt, Disconnect, Identitätswechsel,
   Abbruch und Shutdown; Zeitfortschritt aus Titel-/Originalzeitvertrag,
   niemals aus Zahl präsentierter Bilder. Keine Hardwaretests hier.

Die zunächst offenen Fragen Timeoutbyte -> Zeit und flagabhängige Dauer
wurden im folgenden engen Nachtrag geschlossen. Keine Gameplayheuristik,
kein Vollanalyzer und keine Implementierungsänderung wurden dafür benötigt.

## Nachtrag: Zeit-/Flagvertrag und native Hostabbildung

Dieser Nachtrag ersetzt die oben noch als nächste Arbeit beschriebenen
Zeitsemantiklücken. Read-only-Quelle ist
`C:/Users/ultim/Desktop/KatanaRecomp/reference/flycast-master/`.
Dieser lokale Quellstand hat kein `.git`; deshalb wird kein unbekannter
Commit behauptet. Exakte Dateibindungen und primäre Upstreamlinks:

| Datei / relevante lokale Zeilen | SHA-256 |
| --- | --- |
| [core/hw/maple/maple_devs.cpp](https://github.com/flyinghead/flycast/blob/master/core/hw/maple/maple_devs.cpp), 991..1124 | `f9453b4476dda8c989aabe0516394fbcd3aee2e4cb73f3922bbbb2c4b0921959` |
| [core/hw/maple/maple_cfg.cpp](https://github.com/flyinghead/flycast/blob/master/core/hw/maple/maple_cfg.cpp), 65..73 | `e61488d9972b7369bf2bbd3ebe7546c64ddedc2d453a18a4ca2bed998bd763ba` |
| [core/input/gamepad_device.cpp](https://github.com/flyinghead/flycast/blob/master/core/input/gamepad_device.cpp), 755..764 | `dfefb27a663963a387b1c75a9d575ecef67655d879f0e9ee6710088393ae30a5` |
| [core/sdl/sdl_gamepad.cpp](https://github.com/flyinghead/flycast/blob/master/core/sdl/sdl_gamepad.cpp), 325..393 | `155528650d7be3c62bd51399784bb29d30c63a8e456ec8d4d51f7d3194b77846` |

Die Links identifizieren das Primärprojekt; obige Hashes binden den tatsächlich
gelesenen lokalen Stand. Die beiden Hauptdateien wurden zusätzlich als
Upstreamseiten geöffnet. Keine fremden Source-Dateien verändert.

### Auto-stop und Konfigurationsbyte

Flycasts `maple_sega_purupuru` startet mit `AST=19`, `AST_ms=5000`.
Bei `MDCF_BlockWrite` liest es `dma_buffer_in[10]` als AST und berechnet
`AST_ms=(AST+1)*250`. Somit gilt 0 -> 250 ms, 1 -> 500 ms,
19 -> 5000 ms, 255 -> 64000 ms. CNT bedeutet keine unbegrenzte Laufzeit:
auch kontinuierliche Requests werden durch Auto-stop begrenzt.

Der PAL-Konfigurationscaller ist dazu passend geschlossen:
`8C6042B6` stellt das oben belegte `{0,2,timeout,0}` bereit;
`8C646934` -> `8C660486` -> `8C6604C0` baut zwei Headerwords
(Funktion und Block/Phase) und übergibt bei `8C6604E4` an `8C65F750`
mit `r5=12` (BlockWrite), `r7=2` Headerwords plus dem Payloadpointer.
Payloadbyte 2 liegt nach den 8 Headerbytes bei Offset 10: genau das AST-Byte.
Keine Multiplikation mit Titel- oder Präsentationsframes ist beteiligt.

### Decode des bereits belegten 32-Bit-Packed-Requests

Für `W=out[0] | out[1]<<8 | out[2]<<16 | out[3]<<24` verwendet Flycast:

```text
P = (W >> 8) & 7
N = (W >> 12) & 7
F = (W >> 16) & 255
I = (W >> 24) & 255                 // zunächst positiv, kein int8_t
if (W & 0x8000): I = -I            // INH hat Vorrang
else if (!(W & 0x0800)): I = 0      // ohne EXH keine Neigung
CNT = (W & 1) != 0
M = max(P, N)
power = min((P + N) / 7.0, 1.0)
if F > 0 and (!CNT or I != 0):
    duration_ms = min((1000 * (abs(I)*M if I != 0 else 1)) / F, AST_ms)
else:
    duration_ms = AST_ms
inclination = F / (1000.0 * I * M) if I != 0 and power != 0 else 0
```

Die Dauerdivision ist ganzzahlig und rundet für diese nichtnegativen Werte
nach unten. `inclination` ist eine normierte Steigung pro Millisekunde.
`F` beeinflusst Dauer und Steigung; Flycast leitet F nicht an den normalen
SDL-Doppelmotoraufruf weiter. Die P/N-Namen sind Flycasts Namen: Der PAL-
Encoder legt positive SDK-Stärke in den oberen Amplitudennibble. Für diese
Hostabbildung spielt das Vorzeichen keine Motorseitenrolle, weil P+N zählt.

### Nachgewiesener Sonic-Pfad ohne Neigungsflags

Für `8C109DEC` entstehen ausschließlich Flags 0 oder 1, Byte4=0, F in
[7,59], Stärke in [-4,4] mit +1/-1 -> +2/-2. Deshalb gilt I=0:

- Eingehender Timeoutparameter 0: kein AST-Konfigurationswrite, CNT=0,
  `duration_ms=floor(1000/F)` (16..142 ms; immer unter jedem AST-Limit).
- Eingehender Timeoutparameter T>0: AST=T, CNT=1,
  `duration_ms=(T+1)*250` (500..64000 ms).
- `power=abs(normalisierteStaerke)/7`; beide Hostmotoren erhalten power.
  Stärke 0 ist unmittelbar Stop, unabhängig von der errechneten Restdauer.
- Beispiel `(strength=3,F=20,T=0)` -> power=3/7, 50 ms.
  `(strength=-1,F=20,T=1)` -> normalisiert -2, power=2/7, 500 ms.

Der originale Requesttask bleibt erhalten. Sein aufgeschobener Start ist
unabhängig von der Motorlaufzeit. Für genau diesen geschlossenen Pfad gibt
es keine offene Dauer-/Flagfrage mehr.

### Vollständiger Flycast-SDL-Folgepfad und seine Grenze

`SetVibration` -> `UpdateVibration(playerNum, ...)` -> passende
`GamepadDevice::rumble` -> `SDLGamepad::rumble`. SDL erhält zunächst die
Amplitude für die berechnete Dauer; die normale Doppelmotorvariante setzt
beide Intensitäten gleich. Die alternative Haptic-Sine-Ausgabe verwendet
fest 25 Hz. Frequenz ist also kein direkt reproduzierter Originalmotor-Hz-
Wert im XInput-/normalen SDL-Vertrag.

Für I != 0 setzt SDL `slope=inclination*power` und
`deadline=getTimeMs()+duration_ms`. Nur bei slope>0 verändert
`update_rumble()` den laufenden Pegel: `slope*(deadline-now)` bis 0.
Bei slope<=0 erfolgt im gelesenen SDL-Code keine steigende Rampe; der
initiale Pegel läuft bis zum bereits beauftragten SDL-Timeout. Diese
Asymmetrie und der Sprung bei vorzeitigem AST-Capping dürfen nicht als
physikalisch originalgetreue INH-/EXH-Hüllkurve bezeichnet werden.
Für den oben geschlossenen Soniccaller ist dieser Unterschied ohne Wirkung
(I=0). Ein späterer allgemeiner Neigungsflagpfad kann diese exakt beschriebene
Flycast-SDL-Referenz übernehmen, ohne daraus unbewiesene Hardwaretreue abzuleiten.

### Direkte Übersetzung zur vorhandenen Sonic-Plattform

Die portlokale Bridge kann den SDK-Request sofort auf
`NativePortGamepadVibration{power,power}` abbilden. Der vorhandene
Prozentsatz in `set_gamepad_vibration` skaliert einmal; Flycasts zusätzliche
exponentielle Benutzerskalierung wird nicht übernommen. Letztere ist eine
UI-Präferenz, kein Requestparameter. Bei 100% ist die normalisierte
Amplitude gleich; die vorhandene Plattform rundet auf 16 Bit mit `lround`,
Flycasts SDL-Code trunciert. Keine exakte 16-Bit-Identität behaupten.

`NativePortGamepadVibration` hat keine Dauer. Deshalb muss der native Owner
pro Geräteidentität/Generation eine monotone Deadline führen und bei Ablauf
explizit `{0,0}` senden. Owner-thread-only beachten; Timerablauf rechtzeitig
am bestehenden Hostowner bedienen, nicht `duration` als Framezähler zählen.
Ein späterer Request ersetzt die alte Deadline. Bei Nullpower oder Dauer0
sofort stoppen; bei Menü/Pause, Disconnect, Wechsel, Shutdown ebenfalls.
Die Millisekunden laufen ab der tatsächlichen Ausgabe; Pause stoppt die
Ausgabe ausdrücklich, statt einen Endlosmotor während Guestfreeze zu lassen.

AST wird pro logischem Rumblegerät gehalten, beim neuen Gerät auf 19
initialisiert und durch `8C6042B6` gesetzt. Die bereits belegten Rückgaben
-2/kein Backend, -1/Ausgabefehler und 0/angenommen bleiben erhalten.
Capability, Konfiguration und Requests gemeinsam implementieren, bevor
Capability true wird. Kein Maple-Gerät oder emulierter Bus ist dafür nötig.

Prüfbare synthetische Verträge: AST 0/1/19/255; CNT0/1 mit I0; Nullamplitude;
F0; INH vor EXH; Request ersetzt Deadline; Stop/Disconnect invalidiert den
alten Geräteowner. Hier wurden keine Tests geschrieben oder ausgeführt,
keine Builds gestartet und keine Implementierungsdateien geändert.
