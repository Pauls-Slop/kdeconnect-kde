/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.kdeconnect as KDEConnect

QtObject {
    id: root

    required property KDEConnect.DeviceDbusInterface device

    readonly property bool available: checker.available && !!plugin?.available

    readonly property KDEConnect.PluginChecker pluginChecker: KDEConnect.PluginChecker {
        id: checker
        pluginName: "screenmirror"
        device: root.device
    }

    readonly property KDEConnect.ScreenmirrorDbusInterface plugin:
        checker.available ? KDEConnect.ScreenmirrorDbusInterfaceFactory.create(device.id()) : null
}
