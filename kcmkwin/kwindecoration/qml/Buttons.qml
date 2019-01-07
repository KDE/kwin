/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls 2.0 as QQC2
import QtQuick.Layouts 1.1
import org.kde.kwin.private.kdecoration 1.0 as KDecoration
import org.kde.kquickcontrolsaddons 2.0 as KQuickControlsAddons;

Item {
    objectName: "buttonLayout"
    Layout.preferredHeight: layout.height
    KDecoration.Bridge {
        id: bridgeItem
        plugin: "org.kde.breeze"
    }
    KDecoration.Settings {
        id: settingsItem
        bridge: bridgeItem.bridge
    }
    Rectangle {
        anchors.fill: parent
        border.width: Math.ceil(units.gridUnit / 16.0)
        border.color: highlightColor
        color: baseColor
        ColumnLayout {
            id: layout
            width: parent.width
            height: titlebarRect.height + availableGrid.height + dragHint.height + 5*layout.spacing
            Rectangle {
                id: titlebarRect
                height: buttonPreviewRow.height + units.smallSpacing
                border.width: Math.ceil(units.gridUnit / 16.0)
                border.color: highlightColor
                color: backgroundColor
                Layout.fillWidth: true
                Layout.topMargin: -border.width // prevent a double top border
                RowLayout {
                    id: buttonPreviewRow
                    anchors.top: parent.top;
                    anchors.left: parent.left;
                    anchors.right: parent.right;
                    anchors.margins: units.smallSpacing / 2
                    height: Math.max(units.iconSizes.small, titlebar.implicitHeight) + units.smallSpacing/2
                    ButtonGroup {
                        id: leftButtonsView
                        height: buttonPreviewRow.height
                        model: leftButtons
                        key: "decoButtonLeft"
                    }
                    QQC2.Label {
                        id: titlebar
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        font: titleFont
                        text: i18n("Titlebar")
                    }

                    ButtonGroup {
                        id: rightButtonsView
                        height: buttonPreviewRow.height
                        model: rightButtons
                        key: "decoButtonRight"
                    }
                }
                DropArea {
                    anchors {
                        fill: titlebarRect
                        leftMargin:   -units.iconSizes.small
                        topMargin:    -units.iconSizes.small
                        rightMargin:  +units.iconSizes.small
                        bottomMargin: +units.iconSizes.small
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
                        var index = view.indexAt(point.x, point.y);
                        if (index == -1 && (view.x + view.width <= drag.x)) {
                            index = view.count - 1;
                        }
                        if (drop.keys.indexOf("decoButtonAdd") != -1) {
                            view.model.add(index, drag.source.type);
                        } else if (drop.keys.indexOf("decoButtonLeft") != -1) {
                            if (view == leftButtonsView) {
                                // move in same view
                                if (index != drag.source.itemIndex) {
                                    drag.source.buttonsModel.move(drag.source.itemIndex, index);
                                }
                            } else  {
                                // move to right view
                                view.model.add(index, drag.source.type);
                                drag.source.buttonsModel.remove(drag.source.itemIndex);
                            }
                        } else if (drop.keys.indexOf("decoButtonRight") != -1) {
                            if (view == rightButtonsView) {
                                // move in same view
                                if (index != drag.source.itemIndex) {
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
                id: availableGrid
                Layout.fillWidth: true
                model: availableButtons
                interactive: false
                height: Math.ceil(cellHeight * 3)
                delegate: Item {
                    id: availableDelegate
                    Layout.margins: units.gridUnit
                    width: availableGrid.cellWidth
                    height: availableGrid.cellHeight
                    opacity: (leftButtonsView.dragging || rightButtonsView.dragging) ? 0.25 : 1.0
                    KDecoration.Button {
                        id: availableButton
                        anchors.centerIn: Drag.active ? undefined : parent
                        bridge: bridgeItem.bridge
                        settings: settingsItem
                        type: model["button"]
                        width: units.iconSizes.small
                        height: units.iconSizes.small
                        Drag.keys: [ "decoButtonAdd" ]
                        Drag.active: dragArea.drag.active
                    }
                    QQC2.Label {
                        id: iconLabel
                        text: model["display"]
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignTop
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                        height: 2 * dragHint.implicitHeight + lineHeight
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
                DropArea {
                    anchors.fill: parent
                    keys: [ "decoButtonRemove" ]
                    onEntered: {
                        drag.accept();
                    }
                    onDropped: {
                        drag.source.buttonsModel.remove(drag.source.itemIndex);
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        opacity: (leftButtonsView.dragging || rightButtonsView.dragging) ? 1.0 : 0.0
                        QQC2.Label {
                            text: i18n("Drop here to remove button")
                            font.weight: Font.Bold
                            Layout.bottomMargin: units.smallSpacing
                        }
                        KQuickControlsAddons.QIconItem {
                            id: icon
                            width: units.iconSizes.smallMedium
                            height: units.iconSizes.smallMedium
                            icon: "emblem-remove"
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }
            Text {
                id: dragHint
                opacity: (leftButtonsView.dragging || rightButtonsView.dragging || availableGrid.dragging) ? 0.0 : 0.50
                Layout.fillWidth: true
                Layout.bottomMargin: units.smallSpacing
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                wrapMode: Text.NoWrap
                text: i18n("Drag buttons between here and the titlebar")
            }
        }
    }
}
