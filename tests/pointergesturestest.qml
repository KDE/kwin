/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.0
import org.kde.kwin.tests 1.0

Image {
    source: "../48-apps-kwin.png"
    scale: gesture.scale * gesture.progressScale

    PinchGesture {
        id: gesture
    }
}
