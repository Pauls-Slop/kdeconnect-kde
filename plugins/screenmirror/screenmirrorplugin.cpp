/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "screenmirrorplugin.h"

#include "adbtools.h"
#include "plugin_screenmirror_debug.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <core/device.h>
#include <core/kdeconnectpluginconfig.h>

K_PLUGIN_CLASS_WITH_JSON(ScreenMirrorPlugin, "kdeconnect_screenmirror.json")

static bool isAndroidDevice(const Device *device)
{
    const DeviceType type = device->type();
    return type == DeviceType::Phone || type == DeviceType::Tablet;
}

ScreenMirrorPlugin::ScreenMirrorPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_session(new AdbSession(this))
{
    connect(m_session, &AdbSession::stateChanged, this, &ScreenMirrorPlugin::stateChanged);
    connect(m_session, &AdbSession::pairingSucceeded, this, [this] {
        config()->set(QStringLiteral("paired"), true);
    });
    // Tool paths can be changed in the settings, which affects availability
    connect(config(), &KdeConnectPluginConfig::configChanged, this, &ScreenMirrorPlugin::stateChanged);
}

QString ScreenMirrorPlugin::dbusPath() const
{
    return QLatin1String("/modules/kdeconnect/devices/%1/screenmirror").arg(device()->id());
}

void ScreenMirrorPlugin::connected()
{
    Q_EMIT stateChanged();
}

bool ScreenMirrorPlugin::available() const
{
    return isAndroidDevice(device()) && !adbPath().isEmpty() && !scrcpyPath().isEmpty();
}

bool ScreenMirrorPlugin::paired() const
{
    return config()->getBool(QStringLiteral("paired"), false);
}

bool ScreenMirrorPlugin::active() const
{
    return m_session->state() == AdbSession::State::Running;
}

bool ScreenMirrorPlugin::busy() const
{
    const auto state = m_session->state();
    return state == AdbSession::State::Pairing || state == AdbSession::State::Connecting || state == AdbSession::State::Stopping;
}

QString ScreenMirrorPlugin::lastError() const
{
    return m_error.isEmpty() ? m_session->lastError() : m_error;
}

QString ScreenMirrorPlugin::scrcpyPath() const
{
    const QString configured = config()->getString(QStringLiteral("scrcpyPath"), QString());
    if (!configured.isEmpty()) {
        return QFileInfo(configured).isExecutable() ? configured : QString();
    }
    return QStandardPaths::findExecutable(QStringLiteral("scrcpy"));
}

QString ScreenMirrorPlugin::adbPath() const
{
    const QString configured = config()->getString(QStringLiteral("adbPath"), QString());
    if (!configured.isEmpty()) {
        return QFileInfo(configured).isExecutable() ? configured : QString();
    }
#ifdef Q_OS_WIN
    // The scrcpy release for Windows ships its own adb.exe
    const QString scrcpy = scrcpyPath();
    if (!scrcpy.isEmpty()) {
        const QString bundled = QFileInfo(scrcpy).dir().filePath(QStringLiteral("adb.exe"));
        if (QFileInfo(bundled).isExecutable()) {
            return bundled;
        }
    }
#endif
    return QStandardPaths::findExecutable(QStringLiteral("adb"));
}

QString ScreenMirrorPlugin::adbHome() const
{
#ifdef Q_OS_WIN
    // adb on Windows always uses the user profile, there is no way to isolate the key
    return QString();
#else
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kdeconnect/screenmirror");
    const QString home = base + QLatin1Char('/') + device()->id();
    if (!QDir().mkpath(home)) {
        return QString();
    }
    // Only the owner may read the pairing key
    const auto ownerOnly = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    QFile::setPermissions(base, ownerOnly);
    QFile::setPermissions(home, ownerOnly);
    return home;
#endif
}

AdbSession::Config ScreenMirrorPlugin::sessionConfig() const
{
    AdbSession::Config session;
    session.adbPath = adbPath();
    session.scrcpyPath = scrcpyPath();
    session.adbHome = adbHome();
    session.deviceAddress = device()->getLocalIpAddress();
    session.disableWirelessDebugging = config()->getBool(QStringLiteral("disableWirelessDebugging"), true);
    session.scrcpy.windowTitle = device()->name();
    session.scrcpy.audio = config()->getBool(QStringLiteral("audio"), true);
    session.scrcpy.control = config()->getBool(QStringLiteral("control"), true);
    session.scrcpy.maxSize = config()->getInt(QStringLiteral("maxSize"), 0);
    session.scrcpy.videoBitRateMbps = config()->getInt(QStringLiteral("videoBitRate"), 0);
    session.scrcpy.turnScreenOff = config()->getBool(QStringLiteral("turnScreenOff"), false);
    session.scrcpy.stayAwake = config()->getBool(QStringLiteral("stayAwake"), false);
    return session;
}

bool ScreenMirrorPlugin::checkPrerequisites()
{
    m_error.clear();
    if (!isAndroidDevice(device())) {
        m_error = i18n("Screen mirroring is only supported for Android phones and tablets.");
    } else if (scrcpyPath().isEmpty()) {
        m_error = i18n("scrcpy was not found. Please install it or set its path in the plugin settings.");
    } else if (adbPath().isEmpty()) {
        m_error = i18n("adb was not found. Please install the Android platform tools or set the path in the plugin settings.");
    } else if (device()->getLocalIpAddress().isNull()) {
        m_error = i18n("The device is not connected over the local network.");
    } else if (m_session->state() != AdbSession::State::Idle) {
        m_error = i18n("Another operation is still in progress.");
    }

    if (!m_error.isEmpty()) {
        qCDebug(KDECONNECT_PLUGIN_SCREENMIRROR) << device()->name() << m_error;
        Q_EMIT stateChanged();
        return false;
    }
    return true;
}

bool ScreenMirrorPlugin::pair(const QString &code, int port)
{
    if (!checkPrerequisites()) {
        return false;
    }
    if (!AdbTools::isValidPairingCode(code)) {
        m_error = i18n("The pairing code must consist of six digits.");
        Q_EMIT stateChanged();
        return false;
    }
    if (port < 0 || port > 65535) {
        m_error = i18n("Invalid port.");
        Q_EMIT stateChanged();
        return false;
    }
    m_session->pair(sessionConfig(), code, static_cast<quint16>(port));
    return true;
}

bool ScreenMirrorPlugin::startMirroring()
{
    if (!checkPrerequisites()) {
        return false;
    }
    if (!paired()) {
        m_error = i18n("The device is not paired for screen mirroring yet. Pair it in the plugin settings first.");
        Q_EMIT stateChanged();
        return false;
    }
    m_session->start(sessionConfig());
    return true;
}

void ScreenMirrorPlugin::stop()
{
    m_session->stop();
}

void ScreenMirrorPlugin::unpair()
{
    if (m_session->state() != AdbSession::State::Idle) {
        return;
    }
#ifndef Q_OS_WIN
    // Never touch the shared key in the user profile on Windows
    const QString home = adbHome();
    if (!home.isEmpty()) {
        QDir(home).removeRecursively();
    }
#endif
    config()->set(QStringLiteral("paired"), false);
    m_error.clear();
    Q_EMIT stateChanged();
}

#include "moc_screenmirrorplugin.cpp"
#include "screenmirrorplugin.moc"
