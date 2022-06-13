/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2

import org.kde.kcm 1.6 as KCM
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin.private.kdecoration 1.0 as KDecoration

// Fake Window
Rectangle {
    id: baseLayout

    readonly property int buttonIconSize: Kirigami.Units.iconSizes.medium
    readonly property int titleBarSpacing: Kirigami.Units.largeSpacing
    readonly property bool draggingTitlebarButtons: leftButtonsView.dragging || rightButtonsView.dragging
    readonly property bool hideDragHint: draggingTitlebarButtons || availableButtonsGrid.dragging

    color: palette.base
    radius: Kirigami.Units.smallSpacing

    KDecoration.Bridge {
        id: bridgeItem
        plugin: "org.kde.breeze"
    }
    KDecoration.Settings {
        id: settingsItem
        bridge: bridgeItem.bridge
    }

    ColumnLayout {
        anchors.fill: parent

        // Fake titlebar
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: buttonPreviewRow.implicitHeight + 2 * baseLayout.titleBarSpacing
            radius: Kirigami.Units.smallSpacing
            gradient: Gradient {
                GradientStop { position: 0.0; color: palette.midlight }
                GradientStop { position: 1.0; color: palette.window }
            }

            RowLayout {
                id: buttonPreviewRow
                anchors {
                    margins: baseLayout.titleBarSpacing
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }

                ButtonGroup {
                    id: leftButtonsView
                    iconSize: baseLayout.buttonIconSize
                    model: kcm.leftButtonsModel
                    key: "decoButtonLeft"

                    Rectangle {
                        visible: stateBindingButtonLeft.nonDefaultHighlightVisible
                        anchors.fill: parent
                        Layout.margins: Kirigami.Units.smallSpacing
                        color: "transparent"
                        border.color: Kirigami.Theme.neutralTextColor
                        border.width: 1
                        radius: Kirigami.Units.smallSpacing
                    }

                    KCM.SettingStateBinding {
                        id: stateBindingButtonLeft
                        configObject: kcm.settings
                        settingName: "buttonsOnLeft"
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    font.bold: true
                    text: i18n("Titlebar")
                }
                ButtonGroup {
                    id: rightButtonsView
                    iconSize: baseLayout.buttonIconSize
                    model: kcm.rightButtonsModel
                    key: "decoButtonRight"

                    Rectangle {
                        visible: stateBindingButtonRight.nonDefaultHighlightVisible
                        anchors.fill: parent
                        Layout.margins: Kirigami.Units.smallSpacing
                        color: "transparent"
                        border.color: Kirigami.Theme.neutralTextColor
                        border.width: 1
                        radius: Kirigami.Units.smallSpacing
                    }

                    KCM.SettingStateBinding {
                        id: stateBindingButtonRight
                        configObject: kcm.settings
                        settingName: "buttonsOnRight"
                    }
                }
            }
            DropArea {
                id: titleBarDropArea
                anchors {
                    fill: parent
                    margins: -baseLayout.titleBarSpacing
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
                        if (leftButtonsView.enabled) {
                            view = leftButtonsView;
                        }
                    } else {
                        if (rightButtonsView.enabled) {
                            view = rightButtonsView;
                        }
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
            Layout.fillHeight: true
            Layout.minimumHeight: availableButtonsGrid.cellHeight * 2
            Layout.margins: Kirigami.Units.largeSpacing
            cellWidth: Kirigami.Units.gridUnit * 6
            cellHeight: Kirigami.Units.gridUnit * 6
            model: kcm.availableButtonsModel
            interactive: false

            delegate: ColumnLayout {
                width: availableButtonsGrid.cellWidth - Kirigami.Units.largeSpacing
                height: availableButtonsGrid.cellHeight - Kirigami.Units.largeSpacing
                opacity: baseLayout.draggingTitlebarButtons ? 0.15 : 1.0

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    color: palette.window
                    radius: Kirigami.Units.smallSpacing
                    implicitWidth: baseLayout.buttonIconSize + Kirigami.Units.largeSpacing
                    implicitHeight: baseLayout.buttonIconSize + Kirigami.Units.largeSpacing

                    KDecoration.Button {
                        id: availableButton
                        anchors.centerIn: Drag.active ? undefined : parent
                        bridge: bridgeItem.bridge
                        settings: settingsItem
                        type: model["button"]
                        width: baseLayout.buttonIconSize
                        height: baseLayout.buttonIconSize
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
                QQC2.Label {
                    id: iconLabel
                    text: model["display"]
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
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
                    opacity: baseLayout.draggingTitlebarButtons ? 1.0 : 0.0
                }
            }
        }
        Text {
            id: dragHint
            readonly property real dragHintOpacitiy: enabled ? 1.0 : 0.3
            color: palette.windowText
            opacity: baseLayout.hideDragHint ? 0.0 : dragHintOpacitiy
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit * 3
            horizontalAlignment: Text.AlignHCenter
            text: i18n("Drag buttons between here and the titlebar")
        }
    }
}
