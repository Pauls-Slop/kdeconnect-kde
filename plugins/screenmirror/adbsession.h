/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "adbtools.h"

#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>

#include <functional>

class QProcess;

/**
 * Drives adb and scrcpy for one device: pairing, connecting and the mirroring
 * session itself. Every operation runs its own private adb server, which is
 * killed again when the operation ends.
 */
class AdbSession : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Pairing,
        Connecting,
        Running,
        Stopping,
    };
    Q_ENUM(State)

    struct Config {
        QString adbPath;
        QString scrcpyPath;
        // Used as HOME for adb so the key is kept per device, empty to use the default key
        QString adbHome;
        QHostAddress deviceAddress;
        bool disableWirelessDebugging = true;
        AdbTools::ScrcpyOptions scrcpy;
        int discoveryAttempts = 8;
        int discoveryIntervalMs = 1000;
    };

    explicit AdbSession(QObject *parent = nullptr);
    ~AdbSession() override;

    State state() const
    {
        return m_state;
    }
    QString lastError() const
    {
        return m_lastError;
    }

    /// The code must be valid, see AdbTools::isValidPairingCode. A port of 0 is discovered via mDNS.
    void pair(const Config &config, const QString &code, quint16 port);
    void start(const Config &config);
    /// Stops a running session or cancels pairing/connecting
    void stop();

Q_SIGNALS:
    void stateChanged();
    void pairingSucceeded();

private:
    using AdbCallback = std::function<void(bool ok, const QString &output)>;

    QProcessEnvironment environment(bool forAdb) const;
    void setState(State state);
    bool begin(const Config &config, State state);
    void fail(const QString &error);

    void runAdb(const QStringList &args, const AdbCallback &done, const QByteArray &input = {}, int timeoutMs = 20000);
    void startServer(const std::function<void()> &next);
    void discover(const QString &serviceType, int attemptsLeft, const std::function<void(quint16)> &next);
    void connectDevice(quint16 port);
    void launchScrcpy();
    void endSession();
    void shutdownNow();

    Config m_config;
    State m_state = State::Idle;
    bool m_cancelRequested = false;
    quint16 m_serverPort = 0;
    QString m_serial;
    QString m_lastError;
    QPointer<QProcess> m_scrcpy;
};
