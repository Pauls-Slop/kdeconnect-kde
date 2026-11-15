# Instructions for AI coding agents

This repository is a fork of the [KDE Connect desktop app](https://invent.kde.org/network/kdeconnect-kde).
Its only addition is the screen mirroring plugin in `plugins/screenmirror/`
(scrcpy over Android 11+ wireless debugging). Read `README.md` and
`plugins/screenmirror/README` before changing anything in that area.

> [!IMPORTANT]
> Upstream KDE Connect does **not** accept merge requests that are fully or
> predominantly AI-engineered. Never open merge requests, issues or comments on
> invent.kde.org or bugs.kde.org from this fork. Upstreaming is the human
> maintainer's decision and work.

## Where things are

| Path | Content |
|---|---|
| `plugins/screenmirror/screenmirrorplugin.*` | KDE Connect plugin, D-Bus interface `org.kde.kdeconnect.device.screenmirror`, settings |
| `plugins/screenmirror/adbsession.*` | State machine that drives adb and scrcpy (pairing, connecting, session, cleanup) |
| `plugins/screenmirror/adbtools.*` | Pure helpers (mDNS output parsing, argument building), no side effects |
| `plugins/screenmirror/kdeconnect_screenmirror_config.qml` | Plugin settings page including pairing |
| `dbusinterfaces/`, `declarativeplugin/` | D-Bus proxy class and QML factory for the plugin |
| `app/qml/DevicePage.qml`, `plasmoid/package/contents/ui/` | UI entry points |
| `tests/screenmirror*` | Unit tests; the session test runs against fake adb/scrcpy shell scripts |

## Building and testing

Requires Qt >= 6.9 and KDE Frameworks 6 (e.g. a current Arch Linux container).

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Every change to `adbsession` or `adbtools` needs matching tests in
`tests/screenmirrorsessiontest.cpp` or `tests/screenmirroradbtoolstest.cpp`.
Behaviour of the real adb that the code relies on (environment variables,
output formats, exit codes) should be checked against an actual adb binary,
not assumed.

## Security invariants of the plugin

adb access equals a shell on the phone. Do not weaken any of these without an
explicit decision by the maintainer:

- Only TLS wireless debugging with pairing; never `adb tcpip`.
- A private adb server per operation on a random loopback port, killed when the
  operation ends. Never set `ANDROID_ADB_SERVER_ADDRESS` (adb then treats the
  server as remote and refuses to start it).
- Per-device adb key via `HOME` for adb only (not for scrcpy), owner-only
  permissions. `ADB_VENDOR_KEYS` and `ANDROID_SERIAL` are removed from the
  environment, `ADB_MDNS_AUTO_CONNECT=0`.
- Only the IP of the authenticated KDE Connect link is contacted.
- The pairing code goes to adb on stdin, never on the command line.
- Wireless debugging is turned off on the device when a session ends (default).
- Processes are started with argument lists, never through a shell.

## Code standards

- Follow the existing KDE Connect patterns (see `plugins/virtualmonitor` for a
  plugin that starts external processes); changes must blend in.
- ASCII only in code and comments: use `-`, `->`, `x`, `...` instead of
  unicode equivalents. Translations in `po/` are the exception.
- Keep comments concise. User-visible strings are English and wrapped in
  `i18n`/`i18nd`; add the German translation to `po/de/` as well.
- Prefer reusing existing infrastructure over new subsystems. Pause and ask the
  maintainer before large or invasive changes.

## Commits and pushes

- Do not commit or push without explicit approval from the maintainer for each
  action.
- Use `Assisted-by: <assistant name>` in commit messages, not `Co-authored-by:`.
- Never rewrite published history (no force push to `master`).
