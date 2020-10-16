/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.14
import QtQuick.Dialogs 1.0 as QtDialogs

Loader {
    id: root
    active: false

    property string title : i18n("Select File");
    property string lastFolder : ""
    property bool isSaveDialog : false

    signal fileSelected(string path)

    sourceComponent: QtDialogs.FileDialog {
        id: fileDialog

        title: root.title
        selectExisting: !root.isSaveDialog
        folder: root.lastFolder || shortcuts.home
        nameFilters: [ i18n("KWin Rules (*.kwinrule)") ]
        defaultSuffix: "*.kwinrule"

        Component.onCompleted: {
            open();
        }

        onAccepted: {
            root.lastFolder = folder;
            if (fileUrl != "") {
                root.fileSelected(fileUrl);
            }
            root.active = false;
        }

        onRejected: {
            root.active = false;
        }
    }
}
