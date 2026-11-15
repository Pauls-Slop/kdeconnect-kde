/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QHostAddress>
#include <QString>
#include <QStringList>

// Pure helpers without side effects, kept separate so they can be unit tested.
namespace AdbTools
{
inline const QString TLS_CONNECT_SERVICE = QStringLiteral("_adb-tls-connect._tcp");
inline const QString TLS_PAIRING_SERVICE = QStringLiteral("_adb-tls-pairing._tcp");

struct ScrcpyOptions {
    QString serial;
    QString windowTitle;
    bool audio = true;
    bool control = true;
    int maxSize = 0; // 0: unlimited
    int videoBitRateMbps = 0; // 0: scrcpy default
    bool turnScreenOff = false;
    bool stayAwake = false;
};

/**
 * Parses the output of "adb mdns services" and returns the port of the first
 * service of type @p serviceType announced by @p host, or 0 if there is none.
 * Services announced by any other address are ignored on purpose.
 */
quint16 findServicePort(const QString &mdnsOutput, const QString &serviceType, const QHostAddress &host);

/// Wireless debugging pairing codes are exactly six digits
bool isValidPairingCode(const QString &code);

/// "host:port", with brackets for IPv6
QString hostPort(const QHostAddress &host, quint16 port);

/// IPv4-mapped IPv6 addresses (as used by dual stack sockets) are turned into plain IPv4
QHostAddress normalizedAddress(const QHostAddress &address);

QStringList scrcpyArguments(const ScrcpyOptions &options);

/// True if the output of "adb pair" reports success
bool pairingSucceeded(const QString &output);

/// True if the output of "adb connect" reports success
bool connectSucceeded(const QString &output);
}
