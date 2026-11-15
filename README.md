# KDE Connect mit Bildschirmspiegelung (Fork)

Dieser Fork von [KDE Connect](https://invent.kde.org/network/kdeconnect-kde) (Desktop-App) ergänzt ein Plugin **Bildschirmspiegelung**: Bildschirm und Ton eines Android-Geräts erscheinen in einem Fenster auf dem PC, das Gerät lässt sich dort mit Maus und Tastatur bedienen. Die Arbeit erledigt [scrcpy](https://github.com/Genymobile/scrcpy) über „Debugging über WLAN“ (Android 11 und neuer).

Alle anderen Funktionen entsprechen dem Original. Auf dem Handy wird die normale KDE-Connect-App aus [F-Droid](https://f-droid.org/packages/org.kde.kdeconnect_tp/) oder dem [Play Store](https://play.google.com/store/apps/details?id=org.kde.kdeconnect_tp) benutzt, sie muss nicht verändert werden.

## Voraussetzungen

- Android 11 oder neuer (Ton ebenfalls ab Android 11), mit eingeschalteten Entwickleroptionen
- Handy und PC im selben Netzwerk und in KDE Connect gekoppelt
- Auf dem PC: `scrcpy` und `adb` (Android Platform Tools)
  - Linux: Pakete `scrcpy` und `android-tools` bzw. `adb` der Distribution
  - Windows: die [scrcpy-Release](https://github.com/Genymobile/scrcpy/releases) entpacken; das mitgelieferte `adb.exe` wird automatisch gefunden. Den Pfad zu `scrcpy.exe` in den Plugin-Einstellungen eintragen, falls er nicht im `PATH` liegt.

## Bauen

### Linux

Benötigt Qt ab 6.9 und KDE Frameworks 6 samt Entwicklungspaketen.

```sh
git clone https://github.com/Pauls-Slop/kdeconnect-kde.git
cmake -S kdeconnect-kde -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
pkill kdeconnectd; kdeconnectd &
```

Die eigenen Tests laufen mit `ctest --test-dir build -R screenmirror`.

### Windows

Mit [KDE Craft](https://community.kde.org/Craft) bauen und dabei die Quellen auf diesen Fork umstellen. Der Windows-Build ist bisher ungetestet.

## Benutzung

1. **Plugin aktivieren:** In der KDE-Connect-App das Gerät öffnen, Einstellungen > Plugins > „Bildschirmspiegelung“ einschalten. Das Plugin ist standardmäßig aus und wird nur für Handys und Tablets angeboten.
2. **Einmalig koppeln:** Auf dem Handy Einstellungen > Entwickleroptionen > Debugging über WLAN einschalten und „Gerät über Kopplungscode koppeln“ antippen. Den sechsstelligen Code in den Plugin-Einstellungen eingeben und „Koppeln“ klicken. Der Port wird automatisch erkannt.
3. **Spiegeln:** „Debugging über WLAN“ einschalten (am schnellsten über eine Schnelleinstellungs-Kachel, die sich in den Entwickleroptionen hinzufügen lässt) und dann in der KDE-Connect-App „Bildschirm spiegeln“ wählen oder im Plasma-Applet auf das Bildschirm-Symbol klicken. Fenster schließen oder erneut klicken beendet die Sitzung.

### Einstellungen

| Einstellung | Standard | Bedeutung |
|---|---|---|
| Debugging über WLAN nach der Sitzung ausschalten | an | Schaltet die Funktion auf dem Handy nach jeder Sitzung wieder ab |
| Steuerung mit Maus und Tastatur erlauben | an | Aus = reines Ansehen |
| Ton übertragen | an | Gerätesound läuft über den PC |
| Gerätebildschirm ausschalten / wach halten | aus | Nur mit Steuerung möglich |
| Maximale Auflösung, Video-Bitrate | Original, Standard | Für langsamere Netzwerke |
| Pfade zu scrcpy und adb | PATH | Nur nötig, wenn nicht im PATH |

## Sicherheit

ADB-Zugriff entspricht einer Shell auf dem Gerät. Das Plugin hält diesen Zugriff deshalb so klein wie möglich:

- Nur verschlüsseltes Debugging über WLAN mit Kopplungscode; der ungeschützte Modus `adb tcpip 5555` wird nie verwendet.
- Debugging über WLAN wird nach jeder Sitzung auf dem Gerät abgeschaltet, das Gerät ist dann nicht mehr per ADB erreichbar.
- Jede Sitzung startet einen eigenen adb-Server auf einem zufälligen Port, der nur lokal erreichbar ist und danach beendet wird. Ein adb für Entwicklungszwecke wird nicht berührt.
- Unter Linux und macOS bekommt jedes Gerät einen eigenen adb-Schlüssel in `~/.local/share/kdeconnect/screenmirror/<Geräte-ID>`, lesbar nur für den eigenen Benutzer. „Kopplung entfernen“ löscht ihn. Unter Windows nutzt adb immer den Schlüssel im Benutzerprofil.
- Verbunden wird nur mit der IP-Adresse, unter der KDE Connect das Gerät kennt; mDNS-Ankündigungen anderer Adressen und automatisches Verbinden werden ignoriert. Das Gerätezertifikat wird beim Koppeln festgehalten.
- Der Kopplungscode wird über die Standardeingabe übergeben und erscheint nicht in der Prozessliste.

Restrisiken: Während einer Sitzung können andere Benutzerkonten auf demselben PC den lokalen adb-Server erreichen. In den Entwickleroptionen sollte „Zeitlimit für ADB-Autorisierung deaktivieren“ ausgeschaltet bleiben, damit Android ungenutzte Freigaben nach 7 Tagen entzieht. Um den Zugriff ganz zu entziehen, den PC zusätzlich auf dem Handy unter „Debugging über WLAN > Gekoppelte Geräte“ entfernen.

Details stehen in [`plugins/screenmirror/README`](plugins/screenmirror/README).

## Fehlersuche

- **„Das Gerät wurde nicht gefunden“:** Debugging über WLAN ist auf dem Handy aus, oder Handy und PC sind in verschiedenen Netzen. Manche Router blockieren mDNS zwischen WLAN und LAN.
- **„Das Gerät bietet keine Kopplung an“:** Auf dem Handy muss der Dialog „Gerät über Kopplungscode koppeln“ geöffnet sein, solange auf „Koppeln“ geklickt wird. Alternativ den angezeigten Port von Hand eintragen.
- **„Das Gerät hat die Verbindung abgelehnt“:** Kopplung entfernen und neu koppeln.
- Ausführliches Log:
  ```sh
  QT_LOGGING_RULES="kdeconnect.plugin.screenmirror.debug=true" kdeconnectd
  ```

## Aufbau des Patches

| Pfad | Inhalt |
|---|---|
| `plugins/screenmirror/` | Plugin, adb/scrcpy-Steuerung (`adbsession`), Hilfsfunktionen (`adbtools`), Einstellungsseite |
| `dbusinterfaces/`, `declarativeplugin/` | D-Bus- und QML-Anbindung |
| `app/qml/DevicePage.qml` | Aktion „Bildschirm spiegeln“ in der App |
| `plasmoid/package/contents/ui/` | Schalter im Plasma-Applet |
| `tests/screenmirror*` | Tests mit simuliertem adb und scrcpy |
| `po/de/` | Deutsche Übersetzungen |

## Upstream

Das Original wird auf [invent.kde.org](https://invent.kde.org/network/kdeconnect-kde) entwickelt. Dieser Fork ist kein offizieller Teil von KDE Connect; Fehler im Plugin bitte hier melden, nicht bei KDE.

## Lizenz

[GNU GPL v2](https://www.gnu.org/licenses/gpl-2.0.html) und [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.html), wie das Original.
