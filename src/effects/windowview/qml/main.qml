/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Layouts 1.4
import QtGraphicalEffects 1.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.effects 1.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.KWin.Effect.WindowView 1.0

Item {
    id: container

    required property QtObject effect
    required property QtObject targetScreen
    required property var selectedIds

    property bool animationEnabled: false
    property bool organized: false

    readonly property int animationDuration: PlasmaCore.Units.longDuration

    function start() {
        container.animationEnabled = true;
        container.organized = true;
    }

    function stop() {
        container.organized = false;
    }

    Keys.onEscapePressed: effect.deactivate(animationDuration);

    KWinComponents.DesktopBackgroundItem {
        activity: KWinComponents.Workspace.currentActivity
        desktop: KWinComponents.Workspace.currentVirtualDesktop
        outputName: targetScreen.name

        layer.enabled: true
        layer.effect: FastBlur {
            radius: container.organized ? 64 : 0
            Behavior on radius {
                NumberAnimation { duration: container.animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor
        opacity: container.organized ? 0.75 : 0

        TapHandler {
            onTapped: effect.deactivate(animationDuration);
        }

        Behavior on opacity {
            OpacityAnimator { duration: animationDuration; easing.type: Easing.OutCubic }
        }
    }

    ColumnLayout {
        width: targetScreen.geometry.width
        height: targetScreen.geometry.height
        PlasmaExtras.SearchField {
            id: searchField
            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: PlasmaCore.Units.gridUnit
            Layout.preferredWidth: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
            focus: true
            Keys.priority: Keys.AfterItem
            Keys.onPressed: {
                switch (event.key) {
                case Qt.Key_Down:
                    heap.forceActiveFocus();
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    heap.forceActiveFocus();
                    if (heap.count === 1) {
                        heap.activateIndex(0);
                    }
                    break;
                default:
                    break;
                }
            }
            Keys.onEnterPressed: {
                heap.forceActiveFocus();
                if (heap.count === 1) {
                    heap.activateCurrentClient();
                }
            }
        }
        WindowHeap {
            id: heap
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            padding: PlasmaCore.Units.largeSpacing
            animationDuration: container.animationDuration
            animationEnabled: container.animationEnabled
            dragEnabled: false
            organized: container.organized
            showOnly: container.effect.mode === WindowView.ModeWindowClass ? "activeClass" : selectedIds
            layout: effect.layout
            model: KWinComponents.ClientFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: container.effect.mode == WindowView.ModeCurrentDesktop ? KWinComponents.Workspace.currentVirtualDesktop : undefined
                screenName: targetScreen.name
                clientModel: stackModel
                filter: searchField.text
                minimizedWindows: !effect.ignoreMinimized
                windowType: ~KWinComponents.ClientFilterModel.Dock &
                        ~KWinComponents.ClientFilterModel.Desktop &
                        ~KWinComponents.ClientFilterModel.Notification;
            }
            onActivated: effect.deactivate(animationDuration);
        }
    }
    PlasmaExtras.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (PlasmaCore.Units.gridUnit * 8)
        visible: heap.count === 0
        iconName: "edit-none"
        text: searchField.text.length > 0 ? i18nd("kwin_effects", "No Matches") : i18nd("kwin_effects", "No Windows")
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            desktop: KWinComponents.Workspace.currentVirtualDesktop
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Dock
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            visible: opacity > 0
            opacity: (model.client.hidden || container.organized) ? 0 : 1

            Behavior on opacity {
                NumberAnimation { duration: animationDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    KWinComponents.ClientModel {
        id: stackModel
    }

    Component.onCompleted: start();
}
