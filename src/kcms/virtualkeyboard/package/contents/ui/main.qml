/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


import QtQuick 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.3 as QQC2
import org.kde.kirigami 2.6 as Kirigami
import org.kde.kcm 1.3 as KCM

KCM.GridViewKCM {
    id: root
    KCM.ConfigModule.quickHelp: i18n("This module lets you choose the virtual keyboard to use.")

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
