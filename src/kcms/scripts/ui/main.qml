/*
    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.config
import org.kde.kirigami 2.10 as Kirigami
import org.kde.newstuff as NewStuff
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    implicitWidth: Kirigami.Units.gridUnit * 22
    implicitHeight: Kirigami.Units.gridUnit * 20

    actions: [
        Kirigami.Action {
            icon.name: "document-import"
            text: i18n("Install from File…")
            onTriggered: kcm.importScript()
        },
        NewStuff.Action {
            text: i18nc("@action:button get new KWin scripts", "Get New…")
            visible: KAuthorized.authorize(KAuthorized.GHNS)
            configFile: "kwinscripts.knsrc"
            onEntryEvent: (entry, event) => {
                if (event === NewStuff.Engine.StatusChangedEvent) {
                    kcm.onGHNSEntriesChanged()
                }
            }
        }
    ]

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: kcm.errorMessage || kcm.infoMessage
            type: kcm.errorMessage ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
            text: kcm.errorMessage || kcm.infoMessage
        }

        Kirigami.SearchField {
            Layout.fillWidth: true
            id: searchField
        }
    }

    view: KCMUtils.PluginSelector {
        id: selector
        sourceModel: kcm.model
        query: searchField.text

        delegate: KCMUtils.PluginDelegate {
            onConfigTriggered: kcm.configure(model.config)
            additionalActions: [
                Kirigami.Action {
                    enabled: kcm.canDeleteEntry(model.metaData)
                    icon.name: kcm.pendingDeletions.indexOf(model.metaData) === -1 ? "delete" : "edit-undo"
                    text: i18nc("@info:tooltip", "Delete…")
                    displayHint: Kirigami.DisplayHint.IconOnly

                    onTriggered: kcm.togglePendingDeletion(model.metaData)
                }
            ]
        }
    }
}
