/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "adbtools.h"

#include <QRegularExpression>

namespace AdbTools
{

QHostAddress normalizedAddress(const QHostAddress &address)
{
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4) {
        return QHostAddress(v4);
    }
    return address;
}

static bool parseEndpoint(const QString &text, QHostAddress &host, quint16 &port)
{
    const int colon = text.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return false;
    }
    QString hostText = text.left(colon);
    if (hostText.startsWith(QLatin1Char('[')) && hostText.endsWith(QLatin1Char(']'))) {
        hostText = hostText.mid(1, hostText.size() - 2);
    }
    bool ok = false;
    const uint p = text.mid(colon + 1).toUInt(&ok);
    if (!ok || p == 0 || p > 65535) {
        return false;
    }
    if (!host.setAddress(hostText)) {
        return false;
    }
    port = static_cast<quint16>(p);
    return true;
}

quint16 findServicePort(const QString &mdnsOutput, const QString &serviceType, const QHostAddress &host)
{
    if (host.isNull()) {
        return 0;
    }
    const QHostAddress wanted = normalizedAddress(host);
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));

    const QStringList lines = mdnsOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        // Format: <instance name> <service type>[.] <address>:<port>
        const QStringList fields = line.trimmed().split(whitespace, Qt::SkipEmptyParts);
        if (fields.size() < 3) {
            continue;
        }
        QString type = fields.at(fields.size() - 2);
        if (type.endsWith(QLatin1Char('.'))) {
            type.chop(1);
        }
        if (type != serviceType) {
            continue;
        }
        QHostAddress address;
        quint16 port = 0;
        if (!parseEndpoint(fields.constLast(), address, port)) {
            continue;
        }
        if (normalizedAddress(address).isEqual(wanted)) {
            return port;
        }
    }
    return 0;
}

bool isValidPairingCode(const QString &code)
{
    static const QRegularExpression sixDigits(QRegularExpression::anchoredPattern(QStringLiteral("[0-9]{6}")));
    return sixDigits.match(code).hasMatch();
}

QString hostPort(const QHostAddress &host, quint16 port)
{
    const QHostAddress address = normalizedAddress(host);
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        return QLatin1Char('[') + address.toString() + QLatin1String("]:") + QString::number(port);
    }
    return address.toString() + QLatin1Char(':') + QString::number(port);
}

QStringList scrcpyArguments(const ScrcpyOptions &options)
{
    QStringList args;
    args << QStringLiteral("--serial=") + options.serial;
    if (!options.windowTitle.isEmpty()) {
        args << QStringLiteral("--window-title=") + options.windowTitle;
    }
    if (!options.audio) {
        args << QStringLiteral("--no-audio");
    }
    if (!options.control) {
        args << QStringLiteral("--no-control");
    }
    if (options.maxSize > 0) {
        args << QStringLiteral("--max-size=") + QString::number(options.maxSize);
    }
    if (options.videoBitRateMbps > 0) {
        args << QStringLiteral("--video-bit-rate=") + QString::number(options.videoBitRateMbps) + QLatin1Char('M');
    }
    if (options.turnScreenOff && options.control) {
        args << QStringLiteral("--turn-screen-off");
    }
    if (options.stayAwake && options.control) {
        args << QStringLiteral("--stay-awake");
    }
    return args;
}

bool pairingSucceeded(const QString &output)
{
    return output.contains(QLatin1String("Successfully paired"), Qt::CaseInsensitive);
}

bool connectSucceeded(const QString &output)
{
    // "already connected to ..." also counts, "failed to connect to ..." does not
    return output.contains(QLatin1String("connected to"), Qt::CaseInsensitive) && !output.contains(QLatin1String("failed"), Qt::CaseInsensitive)
        && !output.contains(QLatin1String("cannot"), Qt::CaseInsensitive);
}

}
