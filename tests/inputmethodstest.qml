/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2

GridLayout {
    columns: 2
    Label {
        text: "Normal:"
    }
    TextField {
    }
    Label {
        text: "Digits:"
    }
    TextField {
        inputMethodHints: Qt.ImhDigitsOnly
    }
    Label {
        text: "Numbers:"
    }
    TextField {
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }
    Label {
        text: "Uppercase:"
    }
    TextField {
        inputMethodHints: Qt.ImhUppercaseOnly
    }
    Label {
        text: "Lowercase:"
    }
    TextField {
        inputMethodHints: Qt.ImhLowercaseOnly
    }
    Label {
        text: "Phone:"
    }
    TextField {
        inputMethodHints: Qt.ImhDialableCharactersOnly
    }
    Label {
        text: "Email:"
    }
    TextField {
        inputMethodHints: Qt.ImhEmailCharactersOnly
    }
    Label {
        text: "Url:"
    }
    TextField {
        inputMethodHints: Qt.ImhUrlCharactersOnly
    }
    Label {
        text: "Date:"
    }
    TextField {
        inputMethodHints: Qt.ImhDate
    }
    Label {
        text: "Time:"
    }
    TextField {
        inputMethodHints: Qt.ImhTime
    }
}
