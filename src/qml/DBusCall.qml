// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2025 Harald Sitter <sitter@kde.org>

import QtQuick 2.0
import org.kde.dbusaddons

QtObject {
    id: dbusCall

    property string service
    property string path
    property string dbusInterface
    property string method
    property list<var> arguments

    signal finished(args: list<var>)
    signal failed()

    property DBusInterface iface: DBusInterface {
        bus: "session"
        name: dbusCall.service
        path: dbusCall.path
        iface: dbusCall.dbusInterface
        proxy: null
        propertiesInterface: false
    }

    function call() {
        new Promise((resolve, reject) => {
            iface.asyncCall(method, "_implied_", arguments, resolve, reject)
        }).then((result, ...resolveArgs) => {
            finished([result, ...resolveArgs])
        }).catch((error) => {
            failed()
        })
    }
}
