/*
    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.1

import org.kde.kcm 1.2
import org.kde.kconfig 1.0
import org.kde.kirigami 2.10 as Kirigami
import org.kde.newstuff 1.62 as NewStuff
import org.kde.kcmutils 1.0 as KCMUtils

ScrollViewKCM {
    view: KCMUtils.KPluginSelector {
        id: selector
        model: kcm.effectsModel

        delegate: KCMUtils.KPluginDelegate {
            aboutDialog: selector.aboutDialog
            additionalActions: [
                Kirigami.Action {
                        enabled: kcm.canDeleteEntry(model.metaData)
                        icon.name: kcm.pendingDeletions.indexOf(model.metaData) === -1 ? "delete" : "edit-undo"
                        tooltip: i18nc("@info:tooltip", "Configure...")
                        onTriggered: kcm.togglePendingDeletion(model.metaData)
                }
            ]
        }
    }
    footer: RowLayout {
        Layout.alignment: Qt.AlignRight

        NewStuff.Button {
            text: i18n("Get New KWin Scripts ...")
            visible: KAuthorized.authorize(KAuthorized.GHNS)
            configFile: "kwinscripts.knsrc"
            onEntryEvent: function (entry, event) {
                if (event == 1) { // StatusChangedEvent
                    kcm.onGHNSEntriesChanged()
                }
            }
        }
    }
}
