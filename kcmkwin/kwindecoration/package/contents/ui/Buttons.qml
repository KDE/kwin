/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as Controls

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kirigami 2.5 as Kirigami
import org.kde.kwin.private.kdecoration 1.0 as KDecoration

ColumnLayout {
    Layout.fillWidth: true
    Layout.fillHeight: true
    property int buttonIconSize: units.iconSizes.medium
    property int titleBarSpacing: units.smallSpacing * 2
    readonly property bool draggingTitlebarButtons: leftButtonsView.dragging || rightButtonsView.dragging
    readonly property bool hideDragHint: draggingTitlebarButtons || availableButtonsGrid.dragging

    KDecoration.Bridge {
        id: bridgeItem
        plugin: "org.kde.breeze"
    }
    KDecoration.Settings {
        id: settingsItem
        bridge: bridgeItem.bridge
    }

    Rectangle {
        Layout.fillWidth: true
        color: palette.base
        radius: units.smallSpacing
        height: fakeWindow.height
        Layout.margins: units.smallSpacing

        ColumnLayout {
            id: fakeWindow
            width: parent.width

            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                height: buttonPreviewRow.height + 2 * titleBarSpacing
                radius: units.smallSpacing
                gradient: Gradient {
                    GradientStop { position: 0.0; color: palette.midlight }
                    GradientStop { position: 1.0; color: palette.window }
                }

                RowLayout {
                    id: buttonPreviewRow
                    anchors {
                        margins: titleBarSpacing
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }

                    ButtonGroup {
                        id: leftButtonsView
                        iconSize: buttonIconSize
                        model: kcm.leftButtonsModel
                        key: "decoButtonLeft"
                    }
                    Controls.Label {
                        id: titleBarLabel
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                        text: i18n("Titlebar")
                    }
                    ButtonGroup {
                        id: rightButtonsView
                        iconSize: buttonIconSize
                        model: kcm.rightButtonsModel
                        key: "decoButtonRight"
                    }
                }
                DropArea {
                    id: titleBarDropArea
                    anchors {
                        fill: parent
                        margins: -titleBarSpacing
                    }
                    keys: [ "decoButtonAdd", "decoButtonRight", "decoButtonLeft" ]
                    onEntered: {
                        drag.accept();
                    }
                    onDropped: {
                        var view = undefined;
                        var left = drag.x - (leftButtonsView.x + leftButtonsView.width);
                        var right = drag.x - rightButtonsView.x;
                        if (Math.abs(left) <= Math.abs(right)) {
                            view = leftButtonsView;
                        } else {
                            view = rightButtonsView;
                        }
                        if (!view) {
                            return;
                        }
                        var point = mapToItem(view, drag.x, drag.y);
                        var index = 0
                        for(var childIndex = 0 ; childIndex < (view.count - 1) ; childIndex++) {
                            var child = view.contentItem.children[childIndex]
                            if (child.x > point.x) {
                                break
                            }
                            index = childIndex + 1
                        }
                        if (drop.keys.indexOf("decoButtonAdd") !== -1) {
                            view.model.add(index, drag.source.type);
                        } else if (drop.keys.indexOf("decoButtonLeft") !== -1) {
                            if (view === leftButtonsView) {
                                // move in same view
                                if (index !== drag.source.itemIndex) {
                                    drag.source.buttonsModel.move(drag.source.itemIndex, index);
                                }
                            } else  {
                                // move to right view
                                view.model.add(index, drag.source.type);
                                drag.source.buttonsModel.remove(drag.source.itemIndex);
                            }
                        } else if (drop.keys.indexOf("decoButtonRight") !== -1) {
                            if (view === rightButtonsView) {
                                // move in same view
                                if (index !== drag.source.itemIndex) {
                                    drag.source.buttonsModel.move(drag.source.itemIndex, index);
                                }
                            } else {
                                // move to left view
                                view.model.add(index, drag.source.type);
                                drag.source.buttonsModel.remove(drag.source.itemIndex);
                            }
                        }
                    }
                }
            }
            GridView {
                id: availableButtonsGrid
                property bool dragging: false
                Layout.fillWidth: true
                Layout.minimumHeight: availableButtonsGrid.cellHeight * 2
                Layout.margins: units.largeSpacing
                model: kcm.availableButtonsModel
                interactive: false
                delegate: Item {
                    id: availableDelegate
                    Layout.margins: units.largeSpacing
                    width: availableButtonsGrid.cellWidth
                    height: availableButtonsGrid.cellHeight
                    opacity: draggingTitlebarButtons ? 0.15 : 1.0
                    Rectangle {
                        id: availableButtonFrame
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: palette.window
                        radius: units.smallSpacing
                        width: buttonIconSize + units.largeSpacing
                        height: buttonIconSize + units.largeSpacing

                        KDecoration.Button {
                            id: availableButton
                            anchors.centerIn: Drag.active ? undefined : availableButtonFrame
                            bridge: bridgeItem.bridge
                            settings: settingsItem
                            type: model["button"]
                            width: buttonIconSize
                            height: buttonIconSize
                            Drag.keys: [ "decoButtonAdd" ]
                            Drag.active: dragArea.drag.active
                            Drag.onActiveChanged: availableButtonsGrid.dragging = Drag.active
                            color: palette.windowText
                        }
                        MouseArea {
                            id: dragArea
                            anchors.fill: availableButton
                            drag.target: availableButton
                            cursorShape: Qt.SizeAllCursor
                            onReleased: {
                                if (availableButton.Drag.target) {
                                    availableButton.Drag.drop();
                                } else {
                                    availableButton.Drag.cancel();
                                }
                            }
                        }
                    }
                    Controls.Label {
                        id: iconLabel
                        text: model["display"]
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignTop
                        anchors.top: availableButtonFrame.bottom
                        anchors.topMargin: units.smallSpacing
                        anchors.left: parent.left
                        anchors.right: parent.right
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                        height: 2 * implicitHeight + lineHeight
                    }
                }
                DropArea {
                    anchors.fill: parent
                    keys: [ "decoButtonRemove" ]
                    onEntered: {
                        drag.accept();
                    }
                    onDropped: {
                        drag.source.buttonsModel.remove(drag.source.itemIndex);
                    }
                    Kirigami.Heading {
                        text: i18n("Drop button here to remove it")
                        font.weight: Font.Bold
                        level: 2
                        anchors.centerIn: parent
                        opacity: draggingTitlebarButtons ? 1.0 : 0.0
                    }
                }
            }
            Text {
                id: dragHint
                readonly property real dragHintOpacitiy: enabled ? 1.0 : 0.3
                color: palette.windowText
                opacity: hideDragHint ? 0.0 : dragHintOpacitiy
                Layout.fillWidth: true
                Layout.topMargin: titleBarSpacing
                Layout.bottomMargin: titleBarSpacing
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.NoWrap
                text: i18n("Drag buttons between here and the titlebar")
            }
        }
    }
}
