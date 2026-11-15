/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

// Runs AdbSession against fake adb and scrcpy scripts that log their invocations

#include "../plugins/screenmirror/adbsession.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using State = AdbSession::State;

static const char FAKE_ADB[] = R"(#!/bin/sh
echo "adb|HOME=$HOME|PORT=$ANDROID_ADB_SERVER_PORT|ADDR=$ANDROID_ADB_SERVER_ADDRESS|AUTO=$ADB_MDNS_AUTO_CONNECT|VK=$ADB_VENDOR_KEYS|$*" >> "$FAKE_LOG"
case "$1" in
    start-server|kill-server) exit 0 ;;
    mdns) printf 'List of discovered mdns services\n%s' "$FAKE_MDNS"; exit 0 ;;
    pair)
        read code
        echo "stdin=$code" >> "$FAKE_LOG"
        if [ "$code" = "123456" ]; then echo "Enter pairing code: Successfully paired to $2 [guid=adb-X]"; exit 0; fi
        echo "Enter pairing code: Failed: Wrong password or connection was dropped."; exit 1 ;;
    connect) echo "connected to $2"; exit 0 ;;
    -s)
        case "$3" in
            get-state) echo "$FAKE_STATE"; exit 0 ;;
            shell) exit 0 ;;
        esac ;;
esac
exit 1
)";

static const char FAKE_SCRCPY[] = R"(#!/bin/sh
echo "scrcpy|HOME=$HOME|PORT=$ANDROID_ADB_SERVER_PORT|ADB=$ADB|$*" >> "$FAKE_LOG"
if [ -n "$FAKE_SCRCPY_EXIT" ]; then echo "ERROR: boom"; exit "$FAKE_SCRCPY_EXIT"; fi
trap 'exit 0' TERM
while true; do sleep 0.05; done
)";

class ScreenMirrorSessionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void testPairWithDiscovery();
    void testPairWithExplicitPort();
    void testPairWrongCode();
    void testPairingServiceMissing();
    void testStartAndStop();
    void testKeepWirelessDebugging();
    void testDeviceNotFound();
    void testOtherHostsIgnored();
    void testUnauthorized();
    void testCancelWhileConnecting();
    void testScrcpyFails();
    void testScrcpyWindowClosed();
    void testDestructorCleansUp();
    void testBusyRejectsSecondOperation();

private:
    AdbSession::Config config() const;
    QStringList log() const;
    QStringList logMatching(const QString &needle) const;
    void writeScript(const QString &name, const char *content);

    QTemporaryDir m_dir;
    QString m_log;
};

void ScreenMirrorSessionTest::writeScript(const QString &name, const char *content)
{
    QFile file(m_dir.filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void ScreenMirrorSessionTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    writeScript(QStringLiteral("adb"), FAKE_ADB);
    writeScript(QStringLiteral("scrcpy"), FAKE_SCRCPY);
    QDir().mkpath(m_dir.filePath(QStringLiteral("home")));
    m_log = m_dir.filePath(QStringLiteral("log"));
    qputenv("FAKE_LOG", m_log.toLocal8Bit());
    qputenv("ADB_VENDOR_KEYS", "/should/be/removed");
    qputenv("ANDROID_ADB_SERVER_ADDRESS", "192.0.2.1");
}

void ScreenMirrorSessionTest::init()
{
    QFile::remove(m_log);
    qputenv("FAKE_MDNS",
            "adb-X-1\t_adb-tls-connect._tcp\t127.0.0.1:37123\n"
            "adb-X-1\t_adb-tls-pairing._tcp.\t127.0.0.1:41000\n");
    qputenv("FAKE_STATE", "device");
    qunsetenv("FAKE_SCRCPY_EXIT");
}

AdbSession::Config ScreenMirrorSessionTest::config() const
{
    AdbSession::Config config;
    config.adbPath = m_dir.filePath(QStringLiteral("adb"));
    config.scrcpyPath = m_dir.filePath(QStringLiteral("scrcpy"));
    config.adbHome = m_dir.filePath(QStringLiteral("home"));
    config.deviceAddress = QHostAddress(QStringLiteral("127.0.0.1"));
    config.discoveryAttempts = 2;
    config.discoveryIntervalMs = 10;
    config.scrcpy.windowTitle = QStringLiteral("Phone");
    return config;
}

QStringList ScreenMirrorSessionTest::log() const
{
    QFile file(m_log);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QStringList ScreenMirrorSessionTest::logMatching(const QString &needle) const
{
    return log().filter(needle);
}

void ScreenMirrorSessionTest::testPairWithDiscovery()
{
    AdbSession session;
    QSignalSpy paired(&session, &AdbSession::pairingSucceeded);
    session.pair(config(), QStringLiteral("123456"), 0);
    QCOMPARE(session.state(), State::Pairing);
    QTRY_COMPARE(session.state(), State::Idle);

    QCOMPARE(paired.count(), 1);
    QVERIFY(session.lastError().isEmpty());
    QCOMPARE(logMatching(QStringLiteral("|pair 127.0.0.1:41000")).size(), 1);
    QCOMPARE(logMatching(QStringLiteral("stdin=123456")).size(), 1);
    // Never on the command line
    QCOMPARE(logMatching(QStringLiteral("123456")).size(), 1);

    const QStringList adbCalls = logMatching(QStringLiteral("adb|"));
    QVERIFY(!adbCalls.isEmpty());
    QString port;
    for (const QString &call : adbCalls) {
        QVERIFY2(call.contains(QStringLiteral("HOME=") + config().adbHome + QLatin1Char('|')), qPrintable(call));
        QVERIFY2(call.contains(QStringLiteral("|ADDR=|")), qPrintable(call));
        QVERIFY2(call.contains(QStringLiteral("|AUTO=0|")), qPrintable(call));
        QVERIFY2(call.contains(QStringLiteral("|VK=|")), qPrintable(call));
        QVERIFY2(!call.contains(QStringLiteral("PORT=5037|")), qPrintable(call));
        const QString thisPort = call.section(QLatin1Char('|'), 2, 2);
        if (port.isEmpty()) {
            port = thisPort;
        }
        QCOMPARE(thisPort, port);
    }
    QVERIFY(adbCalls.constLast().endsWith(QStringLiteral("|kill-server")));
    // No connection was made, so nothing to turn off
    QVERIFY(logMatching(QStringLiteral("adb_wifi_enabled")).isEmpty());
}

void ScreenMirrorSessionTest::testPairWithExplicitPort()
{
    qputenv("FAKE_MDNS", "");
    AdbSession session;
    QSignalSpy paired(&session, &AdbSession::pairingSucceeded);
    session.pair(config(), QStringLiteral("123456"), 40404);
    QTRY_COMPARE(session.state(), State::Idle);
    QCOMPARE(paired.count(), 1);
    QCOMPARE(logMatching(QStringLiteral("|pair 127.0.0.1:40404")).size(), 1);
    QVERIFY(logMatching(QStringLiteral("|mdns")).isEmpty());
}

void ScreenMirrorSessionTest::testPairWrongCode()
{
    AdbSession session;
    QSignalSpy paired(&session, &AdbSession::pairingSucceeded);
    session.pair(config(), QStringLiteral("654321"), 0);
    QTRY_COMPARE(session.state(), State::Idle);
    QCOMPARE(paired.count(), 0);
    QVERIFY2(session.lastError().contains(QStringLiteral("Wrong password")), qPrintable(session.lastError()));
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));

    // Invalid codes never reach adb
    QFile::remove(m_log);
    session.pair(config(), QStringLiteral("12345\n6"), 0);
    QCOMPARE(session.state(), State::Idle);
    QVERIFY(!session.lastError().isEmpty());
    QVERIFY(log().isEmpty());
}

void ScreenMirrorSessionTest::testPairingServiceMissing()
{
    qputenv("FAKE_MDNS", "adb-X-1\t_adb-tls-connect._tcp\t127.0.0.1:37123\n");
    AdbSession session;
    session.pair(config(), QStringLiteral("123456"), 0);
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(session.lastError().contains(QStringLiteral("Pair device with pairing code")));
    QCOMPARE(logMatching(QStringLiteral("|mdns services")).size(), 2);
    QVERIFY(logMatching(QStringLiteral("|pair ")).isEmpty());
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testStartAndStop()
{
    AdbSession session;
    session.start(config());
    QCOMPARE(session.state(), State::Connecting);
    QTRY_COMPARE(session.state(), State::Running);

    QCOMPARE(logMatching(QStringLiteral("|connect 127.0.0.1:37123")).size(), 1);
    QCOMPARE(logMatching(QStringLiteral("|-s 127.0.0.1:37123 get-state")).size(), 1);

    QTRY_COMPARE(logMatching(QStringLiteral("scrcpy|")).size(), 1);
    const QString scrcpy = logMatching(QStringLiteral("scrcpy|")).constFirst();
    const QString adbPort = logMatching(QStringLiteral("adb|")).constFirst().section(QLatin1Char('|'), 2, 2);
    // scrcpy talks to the same private server, keeps the real HOME and uses our adb
    QVERIFY2(scrcpy.contains(QLatin1Char('|') + adbPort + QLatin1Char('|')), qPrintable(scrcpy));
    QVERIFY2(!scrcpy.contains(QStringLiteral("HOME=") + config().adbHome), qPrintable(scrcpy));
    QVERIFY2(scrcpy.contains(QStringLiteral("ADB=") + config().adbPath + QLatin1Char('|')), qPrintable(scrcpy));
    QVERIFY2(scrcpy.contains(QStringLiteral("--serial=127.0.0.1:37123 --window-title=Phone")), qPrintable(scrcpy));

    session.stop();
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY2(session.lastError().isEmpty(), qPrintable(session.lastError()));

    const QStringList entries = log();
    QCOMPARE(entries.at(entries.size() - 2).section(QLatin1Char('|'), 6), QStringLiteral("-s 127.0.0.1:37123 shell settings put global adb_wifi_enabled 0"));
    QVERIFY(entries.constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testKeepWirelessDebugging()
{
    AdbSession session;
    AdbSession::Config cfg = config();
    cfg.disableWirelessDebugging = false;
    session.start(cfg);
    QTRY_COMPARE(session.state(), State::Running);
    session.stop();
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(logMatching(QStringLiteral("adb_wifi_enabled")).isEmpty());
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testDeviceNotFound()
{
    qputenv("FAKE_MDNS", "");
    AdbSession session;
    session.start(config());
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(session.lastError().contains(QStringLiteral("Wireless debugging")));
    QVERIFY(logMatching(QStringLiteral("|connect")).isEmpty());
    QVERIFY(logMatching(QStringLiteral("scrcpy|")).isEmpty());
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testOtherHostsIgnored()
{
    qputenv("FAKE_MDNS", "adb-EVIL-1\t_adb-tls-connect._tcp\t127.0.0.2:37123\n");
    AdbSession session;
    session.start(config());
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(logMatching(QStringLiteral("|connect")).isEmpty());
}

void ScreenMirrorSessionTest::testUnauthorized()
{
    qputenv("FAKE_STATE", "unauthorized");
    AdbSession session;
    session.start(config());
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(!session.lastError().isEmpty());
    QVERIFY(logMatching(QStringLiteral("scrcpy|")).isEmpty());
}

void ScreenMirrorSessionTest::testCancelWhileConnecting()
{
    AdbSession session;
    session.start(config());
    session.stop();
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(logMatching(QStringLiteral("scrcpy|")).isEmpty());
    QVERIFY(session.lastError().isEmpty());
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testScrcpyFails()
{
    qputenv("FAKE_SCRCPY_EXIT", "1");
    AdbSession session;
    session.start(config());
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY2(session.lastError().contains(QStringLiteral("boom")), qPrintable(session.lastError()));
    QCOMPARE(logMatching(QStringLiteral("adb_wifi_enabled 0")).size(), 1);
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testScrcpyWindowClosed()
{
    qputenv("FAKE_SCRCPY_EXIT", "0");
    AdbSession session;
    session.start(config());
    QTRY_COMPARE(session.state(), State::Idle);
    QVERIFY(session.lastError().isEmpty());
    QCOMPARE(logMatching(QStringLiteral("adb_wifi_enabled 0")).size(), 1);
}

void ScreenMirrorSessionTest::testDestructorCleansUp()
{
    {
        AdbSession session;
        session.start(config());
        QTRY_COMPARE(session.state(), State::Running);
    }
    QCOMPARE(logMatching(QStringLiteral("adb_wifi_enabled 0")).size(), 1);
    QVERIFY(log().constLast().endsWith(QStringLiteral("|kill-server")));
}

void ScreenMirrorSessionTest::testBusyRejectsSecondOperation()
{
    AdbSession session;
    session.start(config());
    session.pair(config(), QStringLiteral("123456"), 0);
    QVERIFY(!session.lastError().isEmpty());
    QTRY_COMPARE(session.state(), State::Running);
    QVERIFY(logMatching(QStringLiteral("|pair")).isEmpty());
    session.stop();
    QTRY_COMPARE(session.state(), State::Idle);
}

QTEST_GUILESS_MAIN(ScreenMirrorSessionTest)

#include "screenmirrorsessiontest.moc"
