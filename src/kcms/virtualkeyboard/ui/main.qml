/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import QtQuick.Controls as QQC2
import org.kde.kwin.virtualkeyboard

KCM.GridViewKCM {
    id: root

    actions: [
        Kirigami.Action {
            text: i18nc("Selector label", "Show Virtual Keyboard:")

            displayComponent: RowLayout {
                QQC2.ComboBox {
                    id: visibilityComboBox
                    currentIndex: kcm.dbusInterface.mode
                    flat: true
                    model: [
                        { label: i18n("Never show"), value: KWinVirtualKeyboard.VirtualKeyboardMode.Off },
                        { label: i18n("Show with Touch and Tablet"), value: KWinVirtualKeyboard.VirtualKeyboardMode.TouchOnly },
                        { label: i18n("Show with Touch, Tablet and Mouse"), value: KWinVirtualKeyboard.VirtualKeyboardMode.On },
                    ]
                    textRole: "label"
                    valueRole: "value"

                    onActivated: kcm.dbusInterface.mode = currentIndex

                    KCM.SettingHighlighter {
                        highlight: kcm.dbusInterface.mode !== KWinVirtualKeyboard.VirtualKeyboardMode.TouchOnly
                    }
                }
            }
        },
    ]

    view.model: kcm.model
    view.currentIndex: kcm.model.inputMethodIndex(kcm.settings.inputMethod)

    KCM.SettingStateBinding {
        configObject: kcm.settings
        settingName: "InputMethod"
    }

    view.delegate: KCM.GridDelegate {
        text: model.display
        toolTip: model.toolTip

        thumbnailAvailable: model.decoration
        thumbnail: Kirigami.Icon {
            anchors.fill: parent
            source: model.decoration
        }
        onClicked: {
            kcm.settings.inputMethod = model.desktopFileName;
        }
        onDoubleClicked: {
            kcm.save();
        }
    }
}
