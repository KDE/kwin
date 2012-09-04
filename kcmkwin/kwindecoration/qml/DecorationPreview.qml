/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 1.1

Item {
    id: decorationPreview
    AuroraeDecoration {
        id: inactiveDecoration
        active: false
        anchors {
            fill: parent
            leftMargin: 40
            rightMargin: 10
            topMargin: 10
            bottomMargin: 40
        }
    }
    AuroraeDecoration {
        id: activeDecoration
        active: true
        anchors {
            fill: parent
            leftMargin: 10
            rightMargin: 40
            topMargin: 40
            bottomMargin: 10
        }
    }
    Component.onCompleted: {
        inactiveDecoration.source = mainScript;
        activeDecoration.source = mainScript;
    }
}
