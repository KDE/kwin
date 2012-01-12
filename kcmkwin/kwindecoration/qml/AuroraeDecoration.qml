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
    property alias active: decoration.active
    property alias source: auroraeLoader.source
    QtObject {
        id: decoration
        property bool active: false
        property string caption: display
        property int desktop: 1
        property variant icon: "xorg"
        property bool closeable: true
        property bool maximizeable: true
        property bool minimizeable: true
        property bool modal: false
        property bool moveable: true
        property bool onAllDesktops: false
        property bool preview: true
        property bool resizeable: true
        property bool setShade: false
        property bool shade: false
        property bool shadeable: false
        property bool keepAbove: false
        property bool keepBelow: false
        property bool maximized: false
        property bool providesContextHelp: true
        property string leftButtons: "MS"
        property string rightButtons: "HIA__X"
        function titleMouseMoved() {}
    }
    Loader {
        id: auroraeLoader
        anchors.fill: parent
    }
}
