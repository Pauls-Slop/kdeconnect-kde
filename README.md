# KDE Connect with screen mirroring (fork)

This fork of the [KDE Connect desktop app](https://invent.kde.org/network/kdeconnect-kde) adds a **Screen Mirroring** plugin: the screen and audio of an Android device are shown in a window on the computer, and the device can be controlled with mouse and keyboard. The heavy lifting is done by [scrcpy](https://github.com/Genymobile/scrcpy) over wireless debugging (Android 11 and newer).

Everything else is identical to upstream. On the phone the regular KDE Connect app from [F-Droid](https://f-droid.org/packages/org.kde.kdeconnect_tp/) or the [Play Store](https://play.google.com/store/apps/details?id=org.kde.kdeconnect_tp) is used; it does not need to be modified.

## Requirements

- Android 11 or newer with developer options enabled
- Phone and computer on the same network and paired in KDE Connect
- On the computer: `scrcpy` and `adb` (Android platform tools)
  - Linux: the `scrcpy` and `android-tools`/`adb` packages of your distribution
  - Windows: extract the [scrcpy release](https://github.com/Genymobile/scrcpy/releases); the bundled `adb.exe` is found automatically. Set the path to `scrcpy.exe` in the plugin settings if it is not in `PATH`.

## Building

### Linux

Requires Qt 6.9 or newer and KDE Frameworks 6 including development packages.

```sh
git clone https://github.com/Pauls-Slop/kdeconnect-kde.git
cmake -S kdeconnect-kde -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
pkill kdeconnectd; kdeconnectd &
```

The plugin's tests run with `ctest --test-dir build -R screenmirror`.

### Windows

Build with [KDE Craft](https://community.kde.org/Craft), pointing the sources to this fork. The Windows build has not been tested yet.

## Usage

1. **Enable the plugin:** In the KDE Connect app open the device, go to Settings > Plugins and enable "Screen Mirroring". The plugin is disabled by default and only offered for phones and tablets.
2. **Pair once:** On the phone open Settings > Developer options > Wireless debugging, enable it and tap "Pair device with pairing code". Enter the six digit code in the plugin settings and click "Pair". The port is detected automatically.
3. **Mirror:** Enable wireless debugging (quickest via a quick settings tile, which can be added in the developer options), then choose "Mirror screen" in the KDE Connect app or click the screen icon in the Plasma applet. Closing the window or clicking again ends the session.

### Settings

| Setting | Default | Meaning |
|---|---|---|
| Turn off wireless debugging when the session ends | on | Disables wireless debugging on the device after every session |
| Allow controlling the device with mouse and keyboard | on | Off = view only |
| Forward audio | on | Device audio plays on the computer |
| Turn the device screen off / keep it awake | off | Only available with control enabled |
| Maximum resolution, video bit rate | original, default | For slower networks |
| Paths to scrcpy and adb | PATH | Only needed if not in PATH |

## Security

adb access equals a shell on the device, so the plugin keeps it as narrow as possible:

- Only encrypted wireless debugging with a pairing code; the unauthenticated `adb tcpip 5555` mode is never used.
- Wireless debugging is turned off on the device after every session, so the device is no longer reachable via adb.
- Every session starts its own adb server on a random port that is only reachable locally and is killed afterwards. An adb server used for development is not touched.
- On Linux and macOS each device gets its own adb key in `~/.local/share/kdeconnect/screenmirror/<device id>`, readable only by your user. "Forget pairing" deletes it. On Windows adb always uses the key in the user profile.
- Only the IP address under which KDE Connect knows the device is contacted; mDNS announcements from other addresses are ignored and auto-connect is disabled. The device certificate is pinned during pairing.
- The pairing code is passed on stdin and does not show up in the process list.

Remaining risks: while a session is running, other user accounts on the same computer can reach the local adb server. Keep "Disable adb authorization timeout" off in the developer options so Android revokes unused authorizations after 7 days. To revoke access completely, also remove the computer under "Wireless debugging > Paired devices" on the phone.

Details are in [`plugins/screenmirror/README`](plugins/screenmirror/README).

## Troubleshooting

- **"The device could not be found":** wireless debugging is off on the phone, or phone and computer are on different networks. Some routers block mDNS between Wi-Fi and LAN.
- **"The device does not offer pairing":** the "Pair device with pairing code" dialog must be open on the phone while you click "Pair". Alternatively enter the port shown there manually.
- **"The device did not accept the connection":** forget the pairing and pair again.
- Verbose log:
  ```sh
  QT_LOGGING_RULES="kdeconnect.plugin.screenmirror.debug=true" kdeconnectd
  ```

## What the fork changes

| Path | Content |
|---|---|
| `plugins/screenmirror/` | Plugin, adb/scrcpy control (`adbsession`), helpers (`adbtools`), settings page |
| `dbusinterfaces/`, `declarativeplugin/` | D-Bus and QML bindings |
| `app/qml/DevicePage.qml` | "Mirror screen" action in the app |
| `plasmoid/package/contents/ui/` | Toggle in the Plasma applet |
| `tests/screenmirror*` | Tests with fake adb and scrcpy |
| `po/de/` | German translations of the new strings |

## Upstream

KDE Connect is developed on [invent.kde.org](https://invent.kde.org/network/kdeconnect-kde). This fork is not an official part of KDE Connect; please report problems with the plugin here, not to KDE.

## License

[GNU GPL v2](https://www.gnu.org/licenses/gpl-2.0.html) and [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.html), like upstream.
