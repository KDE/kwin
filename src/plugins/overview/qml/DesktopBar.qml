/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core as PlasmaCore

Item {
    id: bar

    readonly property real desktopHeight: PlasmaCore.Units.gridUnit * 4
    readonly property real desktopWidth: desktopHeight * targetScreen.geometry.width / targetScreen.geometry.height
    readonly property real columnHeight: desktopHeight + PlasmaCore.Units.gridUnit

    property QtObject windowModel
    property alias desktopModel: desktopRepeater.model
    property QtObject selectedDesktop: null
    property WindowHeap heap

    implicitHeight: columnHeight + 2 * PlasmaCore.Units.smallSpacing

    Flickable {
        anchors.fill: parent
        leftMargin: Math.max((width - contentWidth) / 2, 0)
        topMargin: Math.max((height - contentHeight) / 2, 0)
        contentWidth: contentItem.childrenRect.width
        contentHeight: contentItem.childrenRect.height
        interactive: contentWidth > width
        clip: true
        flickableDirection: Flickable.HorizontalFlick

        Row {
            spacing: PlasmaCore.Units.largeSpacing

            Repeater {
                id: desktopRepeater

                Column {
                    id: delegate
                    activeFocusOnTab: true
                    width: bar.desktopWidth
                    height: bar.columnHeight
                    spacing: PlasmaCore.Units.smallSpacing

                    required property QtObject desktop
                    required property int index

                    Keys.onEnterPressed: activate();
                    Keys.onReturnPressed: activate();
                    Keys.onSpacePressed: activate();
                    Keys.onDeletePressed: remove();

                    Keys.onPressed: {
                        if (event.key === Qt.Key_F2) {
                            event.accepted = true;
                            label.startEditing();
                        }
                    }

                    Keys.onLeftPressed: nextItemInFocusChain(LayoutMirroring.enabled).forceActiveFocus(Qt.BacktabFocusReason);
                    Keys.onRightPressed: nextItemInFocusChain(!LayoutMirroring.enabled).forceActiveFocus(Qt.TabFocusReason);

                    function activate() {
                        KWinComponents.Workspace.currentDesktop = delegate.desktop;
                    }

                    function remove() {
                        if (desktopRepeater.count === 1) {
                            return;
                        }
                        if (delegate.activeFocus) {
                            if (delegate.index === 0) {
                                desktopRepeater.itemAt(1).forceActiveFocus();
                            } else {
                                desktopRepeater.itemAt(delegate.index - 1).forceActiveFocus();
                            }
                        }
                        bar.desktopModel.remove(delegate.index);
                    }

                    Item {
                        width: bar.desktopWidth
                        height: bar.desktopHeight

                        DesktopView {
                            id: thumbnail

                            width: targetScreen.geometry.width
                            height: targetScreen.geometry.height
                            visible: false
                            windowModel: bar.windowModel
                            desktop: delegate.desktop
                            scale: bar.desktopHeight / targetScreen.geometry.height
                            transformOrigin: Item.TopLeft

                            // Disable the item layer while being scaled.
                            layer.enabled: true
                            layer.textureSize: Qt.size(bar.desktopWidth, bar.desktopHeight)
                        }

                        OpacityMask {
                            anchors.fill: parent
                            cached: true
                            source: thumbnail
                            maskSource: Rectangle {
                                width: bar.desktopWidth
                                height: bar.desktopHeight
                                radius: 3
                            }
                        }

                        Rectangle {
                            readonly property bool active: delegate.activeFocus || dropArea.containsDrag || mouseArea.containsPress || bar.selectedDesktop === delegate.desktop
                            anchors.fill: parent
                            anchors.margins: -border.width
                            radius: 3
                            color: "transparent"
                            border.width: 2
                            border.color: active ? PlasmaCore.ColorScope.highlightColor : PlasmaCore.ColorScope.textColor
                            opacity: dropArea.containsDrag || !active ? 0.5 : 1.0
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            onClicked: {
                                mouse.accepted = true;
                                switch (mouse.button) {
                                case Qt.LeftButton:
                                    delegate.activate();
                                    break;
                                case Qt.MiddleButton:
                                    delegate.remove();
                                    break;
                                }
                            }
                        }

                        Loader {
                            LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
                            active: !heap.dragActive && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && desktopRepeater.count > 1
                            anchors.right: parent.right
                            anchors.top: parent.top
                            sourceComponent: PC3.Button {
                                text: i18nd("kwin", "Delete Virtual Desktop")
                                icon.name: "delete"
                                display: PC3.AbstractButton.IconOnly

                                PC3.ToolTip.text: text
                                PC3.ToolTip.visible: hovered
                                PC3.ToolTip.delay: Kirigami.Units.toolTipDelay

                                onClicked: delegate.remove()
                            }
                        }

                        DropArea {
                            id: dropArea
                            anchors.fill: parent

                            onDropped: drop => {
                                drop.accepted = true;
                                // dragging a KWin::Window
                                if (drag.source.desktops.length === 0 || drag.source.desktops.indexOf(delegate.desktop) !== -1) {
                                    drop.action = Qt.IgnoreAction;
                                    return;
                                }
                                drag.source.desktops = [delegate.desktop];
                            }
                        }
                    }

                    Item {
                        id: label
                        width: bar.desktopWidth
                        height: PlasmaCore.Units.gridUnit
                        state: "normal"

                        PC3.Label {
                            anchors.fill: parent
                            elide: Text.ElideRight
                            text: delegate.desktop.name
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            visible: label.state === "normal"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onPressed: {
                                mouse.accepted = true;
                                label.startEditing();
                            }
                        }

                        Loader {
                            active: label.state === "editing"
                            anchors.fill: parent
                            sourceComponent: PC3.TextField {
                                topPadding: 0
                                bottomPadding: 0
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                text: delegate.desktop.name
                                onEditingFinished: {
                                    delegate.desktop.name = text;
                                    label.stopEditing();
                                }
                                Keys.onEscapePressed: label.stopEditing();
                                Component.onCompleted: forceActiveFocus();
                            }
                        }

                        states: [
                            State {
                                name: "normal"
                            },
                            State {
                                name: "editing"
                            }
                        ]

                        function startEditing() {
                            state = "editing";
                        }
                        function stopEditing() {
                            state = "normal";
                        }
                    }

                    HoverHandler {
                        id: hoverHandler
                    }
                }
            }

            PC3.Button {
                width: bar.desktopWidth
                height: bar.desktopHeight

                text: i18nd("kwin", "Add Virtual Desktop")
                icon.name: "list-add"
                display: PC3.AbstractButton.IconOnly
                opacity: hovered ? 1 : 0.75

                PC3.ToolTip.text: text
                PC3.ToolTip.visible: hovered
                PC3. ToolTip.delay: Kirigami.Units.toolTipDelay
                Accessible.name: text

                Keys.onReturnPressed: action.trigger()
                Keys.onEnterPressed: action.trigger()

                Keys.onLeftPressed: nextItemInFocusChain(LayoutMirroring.enabled).forceActiveFocus(Qt.BacktabFocusReason);
                Keys.onRightPressed: nextItemInFocusChain(!LayoutMirroring.enabled).forceActiveFocus(Qt.TabFocusReason);

                onClicked: desktopModel.create(desktopModel.rowCount())

                DropArea {
                    anchors.fill: parent
                    onEntered: {
                        drag.accepted = desktopModel.rowCount() < 20
                    }
                    onDropped: {
                        const desktop = desktopModel.create(desktopModel.rowCount());
                        drag.source.desktops = [desktop];
                    }
                }
            }
        }
    }
}
