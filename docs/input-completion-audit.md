# Input completion audit — Sage

Stand: 2026-09-12, Quellprüfung des laufenden Options-Batches auf
`enhancements/ingame-settings`. Auftraggeber: Eggman.
Repo: `C:/Users/ultim/Desktop/Sonic Adventure Recompiled`.

Keine Builds, Spielstarts, Hardwareabfragen, Eingaben, Monitorwechsel oder
Änderungen an aktiven Source-Dateien. r354 und seine Saves bleiben unangetastet.
Nur dieser Bericht und `.local/analysis/sage-input-completion.patch` wurden
angelegt. Gemeinsame Quellen waren bereits dirty; Zeilennummern beziehen sich
auf den gelesenen Stand, Funktionsnamen sind die stabileren Suchanker.
Interpolation ist zurückgezogen und gehört nicht zu diesem Vorschlag.

## Ergebnis und Priorität

Integration update: Eggman has applied the phantom-trigger guard, alternate
key/mouse prompt corrections and a byte-bound original rumble bridge for the
existing XInput backend. The audit below records the earlier findings; see
[integration and evidence](options-input-audio-completion.md). Direct Sony
rumble/analog-trigger coverage and baked tutorial hints remain separate gaps.

1. **P1: DualSense-Rechtsstick erzeugt zusätzlich Triggerwerte.** Der separate
   Patch entfernt ausschließlich diese falsche Kopplung. Kleine Integration,
   aber ausdrücklich keine vollständige analoge DualSense-Triggerunterstützung.
2. **P1 Produktumfang: Gameplay-Vibration ist nicht nachgewiesen implementiert.**
   Der Regler skaliert eine vorhandene XInput-Ausgabe-API, für die kein
   Spiel-Aufrufer gefunden wurde. Eine direkt angebundene DualSense hat zudem
   keinen eigenen Motorausgabepfad. Nicht als fertiges Feature ausliefern.
3. **P2: Ursprüngliche Tutorials sind nicht binding-aware.** Ein kleiner,
   konkreter Einstieg ist die A/B-Leiste aus `TUTO_CMN(_E).PRS` im SUMMARY-Modul.
   Das ist von vollständiger Übersetzung sämtlicher Tutorialgrafiken und
   dynamischer Spielhinweise zu trennen.
4. **Hotplug ist vorhanden, nicht neu zu bauen.** Slotbindung, Wiedererkennung,
   Menü-Neutralisierung und optionales Pausieren existieren bereits. Bestimmte
   Mehrgeräte-/Identitätswechsel bleiben gezielt zu prüfen; kein physischer
   Reconnect-Test wurde hier durchgeführt.

## 1. Sicherer Phantomtrigger-Patch

Quellkette:

- `tools/prepare-camera-platform.py:22–43` ergänzt die identity-bound
  DualSense-Recompiled-Kamera auf WinMM **Z/R**. Bestehende Bestätigung:
  `docs/camera-style.md`, Abschnitt DualSense, und
  `.local/camera-dualsense-startup-debug-04.log:12` mit
  `source=dualsense axes=Z/R raw=-1,1`.
- Im erzeugten
  `build-performance/generated/camera-platform/native_port_platform.cpp:2466–2490`
  folgt unmittelbar danach weiterhin der alte `JOYCAPS_HASZ`-Block: negatives
  Z wird `left_trigger_raw`, positives Z `right_trigger_raw`.
- `src/sonic_input.cpp:119–120` erzeugt ab Rawwert >64 synthetische LT/RT-Bits.
  `transform()` übernimmt dieselben analogen Werte entsprechend der physischen
  Bindung. `src/sonic_input_bindings.hpp` bindet standardmäßig L1/LB **oder** LT
  und R1/RB **oder** RT auf die ursprünglichen Triggeraktionen.
- `src/native_title_adapter.cpp:17101–17144` projiziert diese Werte in die
  tatsächlichen SDK-Controllerrecords (+24 rechts, +26 links). Es handelt sich
  daher nicht bloß um ein falsch dargestelltes Menülabel.

Der Patch verändert nur den Generator: Das alte combined-Z-Decoding läuft
nicht mehr für `NativeGamepadSourceKind::DualSense`. Der Test erfolgt nach
bestehender Geräteidentifizierung, nicht anhand eines beliebigen HID-Namens.
Er gilt auch bei Camera style Original: Die physische Bedeutung der Z-Achse
wird nicht durch den Kamerastil zu einem Trigger. Die immutable r354-Kopie
wird dadurch nicht verändert. XInput und DualShock behalten ihre bisherigen
Pfade; L1/R1, Tastaturtrigger, Buttons und die vorhandene Z/R-Korrektur bleiben.

**Offen:** Keine vorhandene Aufnahme belegt beide U/V-Capabilities samt
Neutral-/Endwerten und linker/rechter Zuordnung. Die Kamera-Dokumentation
belegt U als frühere Störquelle, aber keinen vollständigen Triggervertrag.
Der Patch liest deshalb weder U noch V. L2/R2 sind damit nicht als analoge
DualSense-Inputs fertig; unverändert vorhandene L1/R1-/Tastaturalternativen
bleiben nutzbar. Auch die bisherige Sony/XInput-Korrelation vergleicht
Triggerwerte; deren Verhalten bei gleichzeitiger Triggerbetätigung ist danach
nicht als gelöst zu behaupten.

Patchbasis `tools/prepare-camera-platform.py`, SHA-256:
`0c1f1e4bbdc67633705830b3a4a2c2467af1bc41ede970486557e35181746b67`.
Python-Syntax des vorgeschlagenen Generatorstands wurde ausschließlich im
Speicher geprüft. `git apply --check .local/analysis/sage-input-completion.patch`
bestand. Kein Generator ausgeführt, kein Objekt kompiliert, kein Spieltest.

Nach Integration sinnvoll: quellgebundene synthetische Raw-Z/R-Fälle für
beide Kamerastile; Z ganz links/rechts darf keine Trigger erzeugen. Getrennte
XInput-/DualShock-Regressionsfälle. U/V erst nach belegter Gerätezuordnung.

## 2. Was für Vibration tatsächlich fehlt

`set_gamepad_vibration()` in der erzeugten Plattformdatei, Zeilen 2826–2870,
validiert Stärke/Slot, löst ausschließlich einen XInput-Slot auf und ruft
`XInputSetState`. `vibration_xinput_slot()` ab Zeile 3067 akzeptiert entweder
direktes XInput oder einen zuvor korrelierten Sony→XInput-Alias. Ohne Alias
liefert die direkte Sony-Verbindung false; es gibt keinen eigenen DualSense-
USB-/Bluetooth-Ausgabevertrag. Der neue Prozentsatz wird erst unmittelbar
vor diesem XInput-Aufruf angewandt (`prepare-camera-platform.py:60–61`).

Die Suche im aktiven `src`, `launcher`, `tools` sowie in den relevanten
Runtime-/Native-Port-C++-Quellen des gepinnten SDK-Archivs fand keinen
Gameplay-Aufrufer von `set_gamepad_vibration`, nur Deklaration, Implementierungen
und den öffentlichen Wrapper. Daraus folgt eine belegte Integrationslücke im
gelesenen Quellpfad, keine Messung am Controller und keine Aussage darüber,
welche konkreten Originalereignisse bereits Vibration anfordern.

Die nächste erforderliche Verbindung ist **Original-Request → native
Vibrationsanforderung**, nicht ein weiterer Lautstärke-/Stärkeregler:

- `service_sonic_native_input()` (`native_title_adapter.cpp:16949`, Projektion
  ab 17021) deckt native Eingaben und die 52-Byte-SDK-Controllerrecords ab.
  Es erzeugt keine Motoranforderungen. Die Frameersetzung ruft diese Projektion
  ab ca. 39883 anstelle der früheren vollständigen SDK-Peripherieservicefamilie.
  Deren Geräte-/Maple-Unterbaum darf nicht einfach wieder aktiviert werden.
- Im gebundenen `postpal-main-ram.bin` steht bei `8C66D52D` tatsächlich
  `pdVib Ver 1.01 Build:Mar 02 1999 21:20:29`. Das belegt vorhandene SDK-Daten,
  **nicht** einen bereits identifizierten, erreichbaren Start-/Stop-Hook.
- Ein genau bytegebundener Spiel-/pdVib-Start/Stop/Status-Owner samt
  Peripheriefähigkeitsabfrage, Requestformat, Dauer und Rückgabesemantik ist
  in dieser begrenzten Prüfung **nicht geschlossen**. Deshalb wird hier
  absichtlich keine unbewiesene Funktionsadresse als sicherer Hook geliefert.

Umsetzung: erst diesen semantischen Request-/Capability-Vertrag bestimmen,
dann auf dem bestehenden Plattform-Owner an `set_gamepad_vibration` übergeben.
Abbruch, Menüpause, Disconnect, Gerätewechsel und Shutdown müssen einen
Stop liefern; keine Nutzung nach Render-FPS skalierter Vibrationsdauer.
Direkte DualSense-Ausgabe ist anschließend ein eigener Backendbaustein mit
belegtem USB-/Bluetooth-Protokoll. Aufwand mittel bis hoch; sichere
Zwischenlösung ist eine als nicht verfügbar markierte Option. Die genaue
Guest-Grenze bleibt eine Implementierungslücke, kein pauschaler Freigabeblocker
für die übrigen Options-Funktionen.

## 3. Konkreter Tutorial-Owner und kleinster sinnvoller Scope

Quelle der statischen SH-4-Belege ist das vorhandene
`C:/Users/ultim/Desktop/KatanaRecomp/private/diagnostics/r330-progress-graphics-timing-20260909a/SUMMARY.disasm.txt`,
mit zugehörigem decodiertem `r289-global-vector-intake/SUMMARY.bin`.
Disassemblierungsbasis `0C900000`, Runtimealias `8C900000`.
SUMMARY-Identität: `93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949`.
Es wurden nur diese bestehenden Referenzen gelesen, keine Katana-Dateien
verändert. Die alten disasm-Artefakte sind statische Quellen, keine neuen
Laufzeitbelege für den aktuellen Build.

**Gemeinsame Tutorialleiste:**

- SUMMARY `8C900B60` initialisiert den Tutorialzweig. `8C900BFC` liest die
  Textsprache `8C754B3C`; `8C900C0C/8C900C1C` wählen `tuto_cmn` bzw.
  `tuto_cmn_e`. Die bestehende Erwerbsfunktion `8C099690` bindet sie an
  TEXLIST `8C9097D4`.
- Display-Owner `8C9012E8` bindet diese TEXLIST bei `8C9013D4–13DA` über
  `8C608C0C`. `8C901438/143A` bereitet Ordinal 0 vor, `8C9014CA` ruft
  `8C07DFEE` auf (Return `8C9014CE`). Diese einzelne A/B-Leiste ist ein
  konkreter Ersatzkandidat; nicht pauschal alle Sprites.
- Katalog `src/sonic-native-texture-catalog.inc:18158–18159`:
  `TUTO_CMN.PRS`, Ordinal 0 `bar_ab`, GBIX `59472`, 512×32,
  PVRT-SHA `aa1618e787d68b67cffde8ed00757eb8606e35ccb0faedda14a2f9faef206dbe`;
  `TUTO_CMN_E.PRS`, Ordinal 0 `bar_ab`, GBIX `98988F`, 512×32,
  PVRT-SHA `3530dc4c613dcc05dc7272b3d8af13f7edf4faf2f641e2dc4b7d5fc9b51a2c30`.

**Charakterspezifische Inhalte:**

- Derselbe Display-Owner verwendet zuvor `8C07EAA0`, unter anderem bei
  `8C9013AE` und `8C9013D0`. Dieser Wrapper ruft bei `8C07EADC` wiederum
  `8C07DFEE`. Die Charakter-Hintergrundauswahl ist separat `8C9012CA`,
  Namenspointertabelle `8C909780`, TEXLIST-Tabelle `8C909944`.
- Die Tutorialtexte sind eigene `TUTOMSG_*`-Texturen, nicht die Host-Menütexte.
  Beispiel `TUTOMSG_SONIC_E.PRS` (Katalogzeile 18149): 32 Einträge, Ordinal 0
  `padmanu` (128×128), danach `sprf_01_e` bis `sprf_30_e` und `prf_null0`.
  Der `padmanu`-PVRT-SHA ist
  `77777f20e3d8dd83db09e6e25163251f43d08ac5af0fa8c466f7a47928e7805b`.
  Schon diese Familie benötigt andere Behandlung als eine einzelne A/B-Leiste.
  Ein vollständiger sprach-/charakterübergreifender Ersatz wurde nicht geprüft.

**Sichere Integration:**

Ein portlokaler Prompt-Resolver muss erst den ursprünglichen **logischen
Spielinput** bestimmen, dann `settings().bindings[Action::A/B/...]` und den
aktuellen GlyphStyle auflösen. Für ursprüngliche Spielhinweise sind dies A/B,
nicht automatisch die separat belegbaren Host-Aktionen Confirm/Cancel.
Xbox-/PlayStation-Symbole allein reichen bei umbelegten Aktionen nicht.

Die Leiste am belegten Owner/Rücksprung und dem vollständig gebundenen
Archiv/Ordinal ersetzen; Modulidentität und Generation prüfen. Originale
Zustandswechsel und Eingabe bleiben vollständig bestehen. Host-Textlayout kann
die bereits vorhandene Rasterisierung aus `sonic_menu_image.cpp` nutzen;
variable Tastennamen dürfen nicht in eine feste einzelne A-Texturzelle gepresst
werden. Native Ersatzressourcen müssen vom Original-TEXLIST-Lebenszyklus
getrennt bleiben. Keine Originalarchive oder Saves überschreiben.

`sonic_native_texture_catalog.hpp` bietet bereits Archivbindung,
Ordinal-/PVRT-Identität und Sourcepfad; diese verwenden, keinen bloßen GBIX-
oder Namensglobalersatz. Besonders `padmanu` existiert auch außerhalb dieses
Tutorialarchivs mit anderen Abmessungen/Bytes. Die letzte native Draw-Leaf-
Grenze des `8C07DFEE`-Pfads wurde hier nicht vollständig geschlossen; ein
Sprite2D-only-Patch wäre deshalb unbewiesen.

## 4. Spielhinweise sind ein weiterer Datenpfad

Vorhandene dynamische Textkette: Queue `8C055C20`, acht Kontexte ab
`8C788B54`/Count `8C788B74`, originaler Service `8C055B32`
(`native_title_adapter.cpp:16147`, `trace_sonic_native_text_progress`).
Resident `8C049A60` ruft den Fensterzeichner `8C054A00`; dieser erreicht
`sonic_native_draw_pretransformed_dispatch`/SDK `8C63EB08`. Die vorhandene
Untertitelanpassung erkennt die konkreten Returns `8C054AF0`/`8C054B9A`,
Recordtyp und Stackform (`native_title_adapter.cpp:36589–36605`).

Dort liegt beim Draw bereits ein gerastertes Textfenster vor, kein fertiges
Host-Action-Token. Ein globales Ersetzen von Zeichen A/B im Debug-/NINJA-
`draw_glyph_family()` wäre semantisch falsch. Ebenso ist `HINT.PRS` kein
universelles Buttonatlas: seine neun Katalogeinträge sind `tical01..08/tical_l`.
Für eingebettete Hinweise zuerst deren Text-/Kontrollcodes auf der
Queue-/Rasterisierungsebene belegen. Diese exakte Button-Token-Zuordnung ist
noch offen. Umfang mittel bis hoch; nicht mit dem kleinen SUMMARY-Leistenfix
als erledigt deklarieren.

## 5. Hotplug und verbleibende Anzeigen

- Plattformpoll: XInput jedes Mal; Sony WinMM-Endpointänderungen veranlassen
  Identitätsrefresh. Der vorhandene Discoveryworker aktualisiert außerdem
  periodisch (10 s, bei Fehlern 5 s). Eine konstante `joyGetNumDevs()`-Zahl
  bedeutet deshalb nicht, dass Hotplug völlig fehlt.
- Bestehende Identitäten behalten ihre P1–P4-Slots; neue Geräte füllen freie
  Slots. Bei P1-Disconnect wird ein bereits vorhandener P2 nicht automatisch
  zu P1. Das ist bewusste Slotstabilität, keine verlorene Enumeration.
- Generation wechselt bei tatsächlichem Identitätswechsel; korrelierte
  Sony/XInput-Aliase behalten Kontinuität. `note_controller()` liefert Sony-
  Zuordnung für Menüs. `Model::update()` und Quit-Prompt disarmen bei Fokus-
  oder Connectionwechsel. `sonic_menu_runtime.cpp:122–135` pausiert optional
  bei beobachtetem P1-Verlust, soweit der Gameplayzustand dafür freigegeben ist.
- Quellrisiken für gezielte spätere Fixtures: schnelle Identitätswechsel im
  selben WinMM-Slot bis zur nächsten Aktualisierung; Fehlerpublikation leert
  den Sony-Katalog; globale Connectiongeneration disarmt auch bei P2-Wechsel;
  kein belegter nahtloser P1-Wechsel zu einem bereits belegten anderen Slot.
  Keine dieser Situationen wurde als reproduzierter Hardwarefehler ausgegeben.
- Kleine echte Anzeigenlücke: `binding_name()` (`sonic_input.cpp:177`) zeigt
  Keyboard nur aus `b.key`, nicht aus einem gültigen alleinigen `b.alternate`.
  `held()` und die Konfigvalidierung akzeptieren letzteres. Außerdem setzt
  WM_XBUTTONDOWN/UP den letzten Gerätetyp nicht wie die anderen Maustasten.
  Diese Ergänzungen sind klein, aber nicht Teil des priorisierten Triggerpatchs.

Integration und weitere Verifikation liegen bei Eggman. Dieser Audit endet
mit Bericht und separat prüfbarem Patch; kein weiterer PD-/Adressscan.
