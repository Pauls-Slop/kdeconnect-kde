/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "adbsession.h"

#include <KLocalizedString>

#include <QLoggingCategory>
#include <QProcess>
#include <QTcpServer>
#include <QTimer>

Q_LOGGING_CATEGORY(KDECONNECT_SCREENMIRROR_SESSION, "kdeconnect.plugin.screenmirror", QtWarningMsg)

static quint16 pickFreeLocalPort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    return server.serverPort();
}

static QString lastLines(const QString &text, int count)
{
    QStringList lines = text.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    while (lines.size() > count) {
        lines.removeFirst();
    }
    return lines.join(QLatin1Char('\n'));
}

static QStringList disableWirelessDebuggingArgs(const QString &serial)
{
    return {QStringLiteral("-s"),
            serial,
            QStringLiteral("shell"),
            QStringLiteral("settings"),
            QStringLiteral("put"),
            QStringLiteral("global"),
            QStringLiteral("adb_wifi_enabled"),
            QStringLiteral("0")};
}

AdbSession::AdbSession(QObject *parent)
    : QObject(parent)
{
}

AdbSession::~AdbSession()
{
    shutdownNow();
}

QProcessEnvironment AdbSession::environment(bool forAdb) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // A private adb server, independent from the one used by development tools
    env.insert(QStringLiteral("ANDROID_ADB_SERVER_PORT"), QString::number(m_serverPort));
    // The server only listens on loopback by default. Setting an address would make adb treat it as remote.
    env.remove(QStringLiteral("ANDROID_ADB_SERVER_ADDRESS"));
    // Use the built-in mDNS implementation on every platform
    env.insert(QStringLiteral("ADB_MDNS_OPENSCREEN"), QStringLiteral("1"));
    // Only ever connect to the address we explicitly ask for
    env.insert(QStringLiteral("ADB_MDNS_AUTO_CONNECT"), QStringLiteral("0"));
    env.remove(QStringLiteral("ADB_VENDOR_KEYS"));
    env.remove(QStringLiteral("ANDROID_SERIAL"));
    // Used by scrcpy to find adb
    env.insert(QStringLiteral("ADB"), m_config.adbPath);
    if (forAdb && !m_config.adbHome.isEmpty()) {
        // adb keeps its key in $HOME/.android. scrcpy keeps the real HOME, it only talks to the running server.
        env.insert(QStringLiteral("HOME"), m_config.adbHome);
    }
    return env;
}

void AdbSession::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
}

bool AdbSession::begin(const Config &config, State state)
{
    if (m_state != State::Idle) {
        m_lastError = i18n("Another operation is still in progress.");
        Q_EMIT stateChanged();
        return false;
    }
    m_config = config;
    m_lastError.clear();
    m_cancelRequested = false;
    m_serial.clear();
    setState(state);
    return true;
}

void AdbSession::fail(const QString &error)
{
    qCWarning(KDECONNECT_SCREENMIRROR_SESSION) << "Screen mirroring failed:" << error;
    m_lastError = error;
    endSession();
}

void AdbSession::runAdb(const QStringList &args, const AdbCallback &done, const QByteArray &input, int timeoutMs)
{
    auto process = new QProcess(this);
#ifdef Q_OS_UNIX
    process->setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif
    process->setProcessEnvironment(environment(true));
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setProgram(m_config.adbPath);
    process->setArguments(args);

    connect(process, &QProcess::finished, this, [process, done](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(process->readAll());
        process->deleteLater();
        done(status == QProcess::NormalExit && exitCode == 0, output);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            done(false, process->errorString());
        }
    });
    QTimer::singleShot(timeoutMs, process, [process] {
        qCWarning(KDECONNECT_SCREENMIRROR_SESSION) << "adb timed out" << process->arguments();
        process->kill();
    });

    process->start();
    if (!input.isEmpty()) {
        process->write(input);
    }
    process->closeWriteChannel();
}

void AdbSession::startServer(const std::function<void()> &next)
{
    m_serverPort = pickFreeLocalPort();
    if (m_serverPort == 0) {
        fail(i18n("Could not find a free local port for adb."));
        return;
    }
    runAdb({QStringLiteral("start-server")}, [this, next](bool ok, const QString &output) {
        if (!ok) {
            fail(i18n("Could not start adb: %1", lastLines(output, 3)));
            return;
        }
        next();
    });
}

void AdbSession::discover(const QString &serviceType, int attemptsLeft, const std::function<void(quint16)> &next)
{
    if (m_cancelRequested) {
        endSession();
        return;
    }
    runAdb({QStringLiteral("mdns"), QStringLiteral("services")}, [this, serviceType, attemptsLeft, next](bool ok, const QString &output) {
        if (m_cancelRequested) {
            endSession();
            return;
        }
        const quint16 port = ok ? AdbTools::findServicePort(output, serviceType, m_config.deviceAddress) : 0;
        if (port != 0 || attemptsLeft <= 1) {
            next(port);
            return;
        }
        QTimer::singleShot(m_config.discoveryIntervalMs, this, [this, serviceType, attemptsLeft, next] {
            discover(serviceType, attemptsLeft - 1, next);
        });
    });
}

void AdbSession::pair(const Config &config, const QString &code, quint16 port)
{
    if (!AdbTools::isValidPairingCode(code)) {
        m_lastError = i18n("The pairing code must consist of six digits.");
        Q_EMIT stateChanged();
        return;
    }
    if (!begin(config, State::Pairing)) {
        return;
    }

    auto doPair = [this, code](quint16 pairingPort) {
        if (pairingPort == 0) {
            fail(i18n("The device does not offer pairing. On the device, open \"Wireless debugging\" and tap \"Pair device with pairing code\"."));
            return;
        }
        // The code is passed on stdin so it does not show up in the process list
        runAdb(
            {QStringLiteral("pair"), AdbTools::hostPort(m_config.deviceAddress, pairingPort)},
            [this](bool ok, const QString &output) {
                if (!ok || !AdbTools::pairingSucceeded(output)) {
                    fail(i18n("Pairing failed: %1", lastLines(output, 2)));
                    return;
                }
                Q_EMIT pairingSucceeded();
                endSession();
            },
            code.toLatin1() + '\n');
    };

    startServer([this, port, doPair] {
        if (port > 0) {
            doPair(port);
        } else {
            discover(AdbTools::TLS_PAIRING_SERVICE, m_config.discoveryAttempts, doPair);
        }
    });
}

void AdbSession::start(const Config &config)
{
    if (!begin(config, State::Connecting)) {
        return;
    }
    startServer([this] {
        discover(AdbTools::TLS_CONNECT_SERVICE, m_config.discoveryAttempts, [this](quint16 port) {
            if (port == 0) {
                fail(i18n("The device could not be found. Make sure \"Wireless debugging\" is enabled on the device."));
                return;
            }
            connectDevice(port);
        });
    });
}

void AdbSession::connectDevice(quint16 port)
{
    m_serial = AdbTools::hostPort(m_config.deviceAddress, port);
    runAdb({QStringLiteral("connect"), m_serial}, [this](bool ok, const QString &output) {
        if (m_cancelRequested) {
            endSession();
            return;
        }
        if (!ok || !AdbTools::connectSucceeded(output)) {
            fail(i18n("Could not connect to the device: %1", lastLines(output, 2)));
            return;
        }
        runAdb({QStringLiteral("-s"), m_serial, QStringLiteral("get-state")}, [this](bool ok, const QString &output) {
            if (m_cancelRequested) {
                endSession();
                return;
            }
            if (!ok || output.trimmed() != QLatin1String("device")) {
                fail(i18n("The device did not accept the connection. Try pairing again."));
                return;
            }
            launchScrcpy();
        });
    });
}

void AdbSession::launchScrcpy()
{
    AdbTools::ScrcpyOptions options = m_config.scrcpy;
    options.serial = m_serial;

    auto process = new QProcess(this);
#ifdef Q_OS_UNIX
    process->setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif
    process->setProcessEnvironment(environment(false));
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setProgram(m_config.scrcpyPath);
    process->setArguments(AdbTools::scrcpyArguments(options));

    connect(process, &QProcess::started, this, [this] {
        if (m_state == State::Connecting) {
            setState(State::Running);
        }
    });
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(process->readAll());
        process->deleteLater();
        // Exit code 0: window closed, 2: device disconnected, 1: scrcpy failed
        if (!m_cancelRequested && (status != QProcess::NormalExit || exitCode == 1)) {
            m_lastError = i18n("scrcpy stopped unexpectedly: %1", lastLines(output, 3));
            qCWarning(KDECONNECT_SCREENMIRROR_SESSION) << m_lastError;
        }
        endSession();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            fail(i18n("Could not start scrcpy: %1", process->errorString()));
        }
    });

    m_scrcpy = process;
    process->start();
}

void AdbSession::stop()
{
    switch (m_state) {
    case State::Idle:
    case State::Stopping:
        return;
    case State::Pairing:
    case State::Connecting:
        m_cancelRequested = true;
        if (m_scrcpy) {
            m_scrcpy->kill();
        }
        return;
    case State::Running:
        m_cancelRequested = true;
        if (m_scrcpy) {
            // finished() ends the session
            m_scrcpy->terminate();
            QTimer::singleShot(3000, m_scrcpy.data(), [process = m_scrcpy] {
                if (process) {
                    process->kill();
                }
            });
        }
        return;
    }
}

void AdbSession::endSession()
{
    if (m_state == State::Stopping || m_state == State::Idle) {
        return;
    }
    setState(State::Stopping);

    auto finish = [this] {
        m_serverPort = 0;
        m_serial.clear();
        m_cancelRequested = false;
        setState(State::Idle);
    };
    auto killServer = [this, finish] {
        if (m_serverPort == 0) {
            finish();
            return;
        }
        runAdb(
            {QStringLiteral("kill-server")},
            [finish](bool, const QString &) {
                finish();
            },
            {},
            5000);
    };

    if (m_config.disableWirelessDebugging && !m_serial.isEmpty() && m_serverPort != 0) {
        // Turning wireless debugging off closes the attack surface until the user enables it again
        runAdb(
            disableWirelessDebuggingArgs(m_serial),
            [killServer](bool, const QString &) {
                killServer();
            },
            {},
            5000);
    } else {
        killServer();
    }
}

void AdbSession::shutdownNow()
{
    // Used on destruction: no more event loop callbacks, do everything synchronously
    const auto processes = findChildren<QProcess *>();
    for (QProcess *process : processes) {
        process->disconnect(this);
        process->kill();
        process->waitForFinished(1000);
        delete process;
    }

    if (m_serverPort == 0) {
        return;
    }

    auto runSync = [this](const QStringList &args) {
        QProcess process;
        process.setProcessEnvironment(environment(true));
        process.start(m_config.adbPath, args);
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(1000);
        }
    };

    if (m_config.disableWirelessDebugging && !m_serial.isEmpty()) {
        runSync(disableWirelessDebuggingArgs(m_serial));
    }
    runSync({QStringLiteral("kill-server")});
    m_serverPort = 0;
    m_state = State::Idle;
}

#include "moc_adbsession.cpp"
