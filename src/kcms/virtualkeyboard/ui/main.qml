/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

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
            displayComponent: RowLayout {
                QQC2.Label {
                    text: i18nc("Selector label", "Show Virtual Keyboard:")
                }

                QQC2.ComboBox {
                    id: visibilityComboBox
                    currentIndex: kcm.mode
                    flat: true
                    model: [
                        { label: i18n("Never"), value: KWinVirtualKeyboard.VirtualKeyboardMode.Off },
                        { label: i18n("With Touch and Tablet"), value: KWinVirtualKeyboard.VirtualKeyboardMode.TouchOnly },
                        { label: i18n("With Touch, Tablet and Mouse"), value: KWinVirtualKeyboard.VirtualKeyboardMode.On },
                    ]
                    textRole: "label"
                    valueRole: "value"

                    onActivated: kcm.mode = currentIndex

                    KCM.SettingHighlighter {
                        highlight: kcm.mode !== KWinVirtualKeyboard.VirtualKeyboardMode.TouchOnly
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
