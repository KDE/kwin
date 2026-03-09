/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.GridViewKCM {
    id: root

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

    footer: ColumnLayout {
        RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                text: i18n("Always show virtual keyboard when a text field is active")
                checked: kcm.settings.alwaysShowVirtualKeyboard
                onToggled: kcm.settings.alwaysShowVirtualKeyboard = checked

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "AlwaysShowVirtualKeyboard"
                }
            }

            Kirigami.ContextualHelpButton {
                toolTipText: i18nc("@info:tooltip", "By default, virtual keyboard panels are only shown when interacting with a text field via touch input, not when using a mouse.")
            }
        }
    }
}
