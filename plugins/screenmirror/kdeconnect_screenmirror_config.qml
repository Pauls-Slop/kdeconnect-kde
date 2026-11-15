/**
 * SPDX-FileCopyrightText: 2026 Pauls-Slop
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

Kirigami.ScrollablePage {
    id: page
    property string device

    readonly property ScreenmirrorDbusInterface mirror: ScreenmirrorDbusInterfaceFactory.create(device)

    KdeConnectPluginConfig {
        id: config
        deviceId: page.device
        pluginName: "kdeconnect_screenmirror"
    }

    Component.onCompleted: {
        audio.checked = config.getBool("audio", true)
        control.checked = config.getBool("control", true)
        turnScreenOff.checked = config.getBool("turnScreenOff", false)
        stayAwake.checked = config.getBool("stayAwake", false)
        disableWirelessDebugging.checked = config.getBool("disableWirelessDebugging", true)
        maxSize.value = config.getInt("maxSize", 0)
        videoBitRate.value = config.getInt("videoBitRate", 0)
        scrcpyPath.text = config.getString("scrcpyPath", "")
        adbPath.text = config.getString("adbPath", "")
    }

    Kirigami.FormLayout {

        Kirigami.Separator {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Pairing")
            Kirigami.FormData.isSection: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Status:")
            text: page.mirror.paired ? i18nd("kdeconnect-plugins", "Paired") : i18nd("kdeconnect-plugins", "Not paired")
        }

        QQC2.Label {
            visible: !page.mirror.paired
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            wrapMode: Text.WordWrap
            text: i18nd("kdeconnect-plugins", "On the device, open Settings > Developer options > Wireless debugging, enable it and tap \"Pair device with pairing code\". Then enter the six digit code here.")
        }

        QQC2.TextField {
            id: code
            visible: !page.mirror.paired
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Pairing code:")
            inputMethodHints: Qt.ImhDigitsOnly
            maximumLength: 6
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
            placeholderText: "123456"
        }

        QQC2.TextField {
            id: pairingPort
            visible: !page.mirror.paired
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Pairing port:")
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 1; top: 65535 }
            placeholderText: i18nd("kdeconnect-plugins", "Detect automatically")
        }

        QQC2.Button {
            visible: !page.mirror.paired
            enabled: code.text.length === 6 && !page.mirror.busy
            text: page.mirror.busy ? i18nd("kdeconnect-plugins", "Pairing...") : i18nd("kdeconnect-plugins", "Pair")
            icon.name: "network-connect"
            onClicked: page.mirror.pair(code.text, pairingPort.text.length > 0 ? parseInt(pairingPort.text) : 0)
        }

        QQC2.Button {
            visible: page.mirror.paired
            enabled: !page.mirror.busy && !page.mirror.active
            text: i18nd("kdeconnect-plugins", "Forget pairing")
            icon.name: "edit-delete-remove"
            onClicked: page.mirror.unpair()
        }

        QQC2.Label {
            visible: page.mirror.paired
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            text: i18nd("kdeconnect-plugins", "To fully revoke access, also remove this computer under \"Wireless debugging > Paired devices\" on the device.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: text.length > 0 && !page.mirror.busy
            text: page.mirror.lastError
        }

        Kirigami.Separator {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Security")
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: disableWirelessDebugging
            text: i18nd("kdeconnect-plugins", "Turn off wireless debugging on the device when the session ends")
            onClicked: config.set("disableWirelessDebugging", checked)
        }

        QQC2.CheckBox {
            id: control
            text: i18nd("kdeconnect-plugins", "Allow controlling the device with mouse and keyboard")
            onClicked: config.set("control", checked)
        }

        Kirigami.Separator {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Session")
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: audio
            text: i18nd("kdeconnect-plugins", "Forward audio (Android 11 or newer)")
            onClicked: config.set("audio", checked)
        }

        QQC2.CheckBox {
            id: turnScreenOff
            enabled: control.checked
            text: i18nd("kdeconnect-plugins", "Turn the device screen off while mirroring")
            onClicked: config.set("turnScreenOff", checked)
        }

        QQC2.CheckBox {
            id: stayAwake
            enabled: control.checked
            text: i18nd("kdeconnect-plugins", "Keep the device awake while mirroring")
            onClicked: config.set("stayAwake", checked)
        }

        QQC2.SpinBox {
            id: maxSize
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Maximum resolution:")
            from: 0
            to: 4096
            stepSize: 128
            editable: true
            textFromValue: (value, locale) => value === 0 ? i18nd("kdeconnect-plugins", "Original") : value + " px"
            valueFromText: (text, locale) => parseInt(text) || 0
            onValueModified: config.set("maxSize", value)
        }

        QQC2.SpinBox {
            id: videoBitRate
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Video bit rate:")
            from: 0
            to: 64
            editable: true
            textFromValue: (value, locale) => value === 0 ? i18nd("kdeconnect-plugins", "Default") : value + " Mbit/s"
            valueFromText: (text, locale) => parseInt(text) || 0
            onValueModified: config.set("videoBitRate", value)
        }

        Kirigami.Separator {
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "Programs")
            Kirigami.FormData.isSection: true
        }

        QQC2.TextField {
            id: scrcpyPath
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "scrcpy:")
            placeholderText: i18nd("kdeconnect-plugins", "Search in PATH")
            onEditingFinished: config.set("scrcpyPath", text.trim())
        }

        QQC2.TextField {
            id: adbPath
            Kirigami.FormData.label: i18nd("kdeconnect-plugins", "adb:")
            placeholderText: i18nd("kdeconnect-plugins", "Search in PATH")
            onEditingFinished: config.set("adbPath", text.trim())
        }
    }
}
