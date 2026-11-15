/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "../plugins/screenmirror/adbtools.h"

#include <QTest>

class ScreenMirrorAdbToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFindServicePort();
    void testFindServicePortIgnoresOtherHosts();
    void testPairingCode();
    void testHostPort();
    void testScrcpyArguments();
    void testOutputParsing();
};

static const QString MDNS_OUTPUT = QStringLiteral(
    "List of discovered mdns services\n"
    "adb-1A2B3C-xyz12\t_adb-tls-connect._tcp\t192.168.1.50:37123\n"
    "adb-1A2B3C-xyz12\t_adb-tls-pairing._tcp.\t192.168.1.50:41000\n"
    "adb-OTHER-abc\t_adb-tls-connect._tcp\t192.168.1.66:40000\n");

void ScreenMirrorAdbToolsTest::testFindServicePort()
{
    const QHostAddress phone(QStringLiteral("192.168.1.50"));
    QCOMPARE(AdbTools::findServicePort(MDNS_OUTPUT, AdbTools::TLS_CONNECT_SERVICE, phone), quint16(37123));
    // Trailing dot in the service type
    QCOMPARE(AdbTools::findServicePort(MDNS_OUTPUT, AdbTools::TLS_PAIRING_SERVICE, phone), quint16(41000));
    // IPv4-mapped address as reported by dual stack sockets
    QCOMPARE(AdbTools::findServicePort(MDNS_OUTPUT, AdbTools::TLS_CONNECT_SERVICE, QHostAddress(QStringLiteral("::ffff:192.168.1.50"))), quint16(37123));
    // IPv6
    const QString v6 = QStringLiteral("adb-X-1\t_adb-tls-connect._tcp\t[fe80::1]:5555\n");
    QCOMPARE(AdbTools::findServicePort(v6, AdbTools::TLS_CONNECT_SERVICE, QHostAddress(QStringLiteral("fe80::1"))), quint16(5555));
}

void ScreenMirrorAdbToolsTest::testFindServicePortIgnoresOtherHosts()
{
    QCOMPARE(AdbTools::findServicePort(MDNS_OUTPUT, AdbTools::TLS_CONNECT_SERVICE, QHostAddress(QStringLiteral("192.168.1.51"))), quint16(0));
    QCOMPARE(AdbTools::findServicePort(MDNS_OUTPUT, AdbTools::TLS_CONNECT_SERVICE, QHostAddress()), quint16(0));
    QCOMPARE(AdbTools::findServicePort(QString(), AdbTools::TLS_CONNECT_SERVICE, QHostAddress(QStringLiteral("192.168.1.50"))), quint16(0));
    // Garbage and out of range ports
    const QString bad = QStringLiteral("x\t_adb-tls-connect._tcp\t192.168.1.50:99999\nx _adb-tls-connect._tcp 192.168.1.50:abc\n");
    QCOMPARE(AdbTools::findServicePort(bad, AdbTools::TLS_CONNECT_SERVICE, QHostAddress(QStringLiteral("192.168.1.50"))), quint16(0));
}

void ScreenMirrorAdbToolsTest::testPairingCode()
{
    QVERIFY(AdbTools::isValidPairingCode(QStringLiteral("123456")));
    QVERIFY(!AdbTools::isValidPairingCode(QStringLiteral("12345")));
    QVERIFY(!AdbTools::isValidPairingCode(QStringLiteral("1234567")));
    QVERIFY(!AdbTools::isValidPairingCode(QStringLiteral("12345a")));
    QVERIFY(!AdbTools::isValidPairingCode(QStringLiteral("123456\n")));
    QVERIFY(!AdbTools::isValidPairingCode(QString()));
}

void ScreenMirrorAdbToolsTest::testHostPort()
{
    QCOMPARE(AdbTools::hostPort(QHostAddress(QStringLiteral("10.0.0.2")), 1234), QStringLiteral("10.0.0.2:1234"));
    QCOMPARE(AdbTools::hostPort(QHostAddress(QStringLiteral("::ffff:10.0.0.2")), 1234), QStringLiteral("10.0.0.2:1234"));
    QCOMPARE(AdbTools::hostPort(QHostAddress(QStringLiteral("fe80::1")), 1234), QStringLiteral("[fe80::1]:1234"));
}

void ScreenMirrorAdbToolsTest::testScrcpyArguments()
{
    AdbTools::ScrcpyOptions options;
    options.serial = QStringLiteral("10.0.0.2:1234");
    options.windowTitle = QStringLiteral("Pixel");
    QCOMPARE(AdbTools::scrcpyArguments(options), (QStringList{QStringLiteral("--serial=10.0.0.2:1234"), QStringLiteral("--window-title=Pixel")}));

    options.audio = false;
    options.maxSize = 1024;
    options.videoBitRateMbps = 8;
    options.turnScreenOff = true;
    options.stayAwake = true;
    const QStringList args = AdbTools::scrcpyArguments(options);
    QVERIFY(args.contains(QStringLiteral("--no-audio")));
    QVERIFY(args.contains(QStringLiteral("--max-size=1024")));
    QVERIFY(args.contains(QStringLiteral("--video-bit-rate=8M")));
    QVERIFY(args.contains(QStringLiteral("--turn-screen-off")));
    QVERIFY(args.contains(QStringLiteral("--stay-awake")));

    // These options need control, scrcpy refuses to start otherwise
    options.control = false;
    const QStringList viewOnly = AdbTools::scrcpyArguments(options);
    QVERIFY(viewOnly.contains(QStringLiteral("--no-control")));
    QVERIFY(!viewOnly.contains(QStringLiteral("--turn-screen-off")));
    QVERIFY(!viewOnly.contains(QStringLiteral("--stay-awake")));
}

void ScreenMirrorAdbToolsTest::testOutputParsing()
{
    QVERIFY(AdbTools::pairingSucceeded(QStringLiteral("Enter pairing code: Successfully paired to 10.0.0.2:41000 [guid=adb-1A2B3C-xyz12]")));
    QVERIFY(!AdbTools::pairingSucceeded(QStringLiteral("Enter pairing code: Failed: Wrong password or connection was dropped.")));

    QVERIFY(AdbTools::connectSucceeded(QStringLiteral("connected to 10.0.0.2:37123")));
    QVERIFY(AdbTools::connectSucceeded(QStringLiteral("already connected to 10.0.0.2:37123")));
    QVERIFY(!AdbTools::connectSucceeded(QStringLiteral("failed to connect to '10.0.0.2:37123': Connection refused")));
    QVERIFY(!AdbTools::connectSucceeded(QStringLiteral("cannot connect to 10.0.0.2:37123: No route to host")));
}

QTEST_GUILESS_MAIN(ScreenMirrorAdbToolsTest)

#include "screenmirroradbtoolstest.moc"
