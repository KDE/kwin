/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtCore
import QtQuick
import QtQuick.Dialogs as QtDialogs

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
        fileMode: root.isSaveDialog ? FileDialog.SaveFile : FileDialog.OpenFile
        currentFolder: root.lastFolder || StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        nameFilters: [ i18n("KWin Rules (*.kwinrule)") ]
        defaultSuffix: "*.kwinrule"

        Component.onCompleted: {
            open();
        }

        onAccepted: {
            root.lastFolder = currentFolder;
            if (selectedFile != "") {
                root.fileSelected(selectedFile);
            }
            root.active = false;
        }

        onRejected: {
            root.active = false;
        }
    }
}
