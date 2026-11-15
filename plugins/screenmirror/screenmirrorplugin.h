/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "adbsession.h"

#include <core/kdeconnectplugin.h>

/**
 * Mirrors the screen and audio of an Android device using scrcpy over
 * Android 11+ wireless debugging (TLS). See README for the security model.
 */
class ScreenMirrorPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.screenmirror")
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)

public:
    explicit ScreenMirrorPlugin(QObject *parent, const QVariantList &args);

    /// Starts a mirroring session. Returns false if it could not even be started, see lastError.
    Q_SCRIPTABLE bool startMirroring();

    /// Stops the running session, or cancels one that is being set up
    Q_SCRIPTABLE void stop();

    /**
     * Pairs with the device using the six digit code shown in
     * "Wireless debugging -> Pair device with pairing code".
     * If @p port is 0 the pairing port is discovered via mDNS.
     */
    Q_SCRIPTABLE bool pair(const QString &code, int port);

    /// Forgets the local pairing key of this device
    Q_SCRIPTABLE void unpair();

    bool available() const;
    bool paired() const;
    bool active() const;
    bool busy() const;
    QString lastError() const;

    void connected() override;
    QString dbusPath() const override;

Q_SIGNALS:
    Q_SCRIPTABLE void stateChanged();

private:
    QString adbPath() const;
    QString scrcpyPath() const;
    QString adbHome() const;
    bool checkPrerequisites();
    AdbSession::Config sessionConfig() const;

    AdbSession *const m_session;
    QString m_error;
};
