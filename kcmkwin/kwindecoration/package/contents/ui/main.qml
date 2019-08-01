/*
   Copyright (c) 2019 Valerio Pilo <vpilo@coldshock.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as Controls
import org.kde.kcm 1.1 as KCM
import org.kde.kconfig 1.0 // for KAuthorized
import org.kde.kirigami 2.4 as Kirigami

Kirigami.Page {
    KCM.ConfigModule.quickHelp: i18n("This module lets you configure the window decorations.")
    KCM.ConfigModule.buttons: KCM.ConfigModule.Help | KCM.ConfigModule.Default | KCM.ConfigModule.Apply
    title: kcm.name

    SystemPalette {
        id: palette
        colorGroup: SystemPalette.Active
    }

    // To match SimpleKCM's borders of Page + headerParent/footerParent (also specified in raw pixels)
    leftPadding: Kirigami.Settings.isMobile ? 0 : 8
    topPadding: leftPadding
    rightPadding: leftPadding
    bottomPadding: leftPadding

    implicitWidth: tabLayout.implicitWidth
    implicitHeight: tabLayout.implicitHeight

    // TODO: replace this TabBar-plus-Frame-in-a-ColumnLayout with whatever shakes
    // out of https://bugs.kde.org/show_bug.cgi?id=394296
    ColumnLayout {
        id: tabLayout

        // Tab styles generally assume that they're touching the inner layout,
        // not the frame, so we need to move the tab bar down a pixel and make
        // sure it's drawn on top of the frame
        Layout.bottomMargin: -1
        z: 1
        anchors.fill: parent
        spacing: 0
        Controls.TabBar {
            id: tabBar
            Layout.fillWidth: true

            Controls.TabButton {
                text: i18nc("tab label", "Theme")
            }

            Controls.TabButton {
                text: i18nc("tab label", "Titlebar Buttons")
            }
        }
        Controls.Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                anchors.fill: parent

                currentIndex: tabBar.currentIndex

                ColumnLayout {
                    Themes {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    RowLayout {
                        Controls.CheckBox {
                            id: borderSizeAutoCheckbox
                            text: i18nc("checkbox label", "Use theme's default window border size")
                            checked: kcm.borderSizeAuto
                            onCheckedChanged: {
                                kcm.borderSizeAuto = checked;
                                borderSizeComboBox.autoBorderUpdate()
                            }
                        }
                        Controls.ComboBox {
                            id: borderSizeComboBox
                            enabled: !borderSizeAutoCheckbox.checked
                            model: kcm.borderSizesModel
                            onActivated: {
                                kcm.borderSize = currentIndex
                            }
                            function autoBorderUpdate() {
                                if (borderSizeAutoCheckbox.checked) {
                                    currentIndex = kcm.recommendedBorderSize
                                } else {
                                    currentIndex = kcm.borderSize
                                }
                            }

                            Connections {
                                target: kcm
                                onThemeChanged: borderSizeComboBox.autoBorderUpdate()
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Controls.Button {
                            text: i18nc("button text", "Get New Window Decorations...")
                            icon.name: "get-hot-new-stuff"
                            onClicked: kcm.getNewStuff(this)
                            visible: KAuthorized.authorize("ghns")
                        }
                    }
                }

                ColumnLayout {
                    Buttons {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    Controls.CheckBox {
                        id: closeOnDoubleClickOnMenuCheckBox
                        text: i18nc("checkbox label", "Close windows by double clicking the menu button")
                        checked: kcm.closeOnDoubleClickOnMenu
                        onToggled: {
                            kcm.closeOnDoubleClickOnMenu = checked
                            infoLabel.visible = checked
                        }
                    }

                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        id: infoLabel
                        type: Kirigami.MessageType.Information
                        text: i18nc("popup tip", "Close by double clicking: Keep the window's Menu button pressed until it appears.")
                        showCloseButton: true
                        visible: false
                    }

                    Controls.CheckBox {
                        id: showToolTipsCheckBox
                        text: i18nc("checkbox label", "Show titlebar button tooltips")
                        checked: kcm.showToolTips
                        onToggled: kcm.showToolTips = checked
                    }
                }
            }
        }
    }
}
