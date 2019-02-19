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

    ColumnLayout {
        id: tabLayout
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
                        Controls.Label {
                            Layout.alignment: Qt.AlignRight
                            text: i18nc("combobox label", "Window border size:")
                        }

                        Controls.ComboBox {
                            id: borderSizeComboBox
                            model: kcm.borderSizesModel
                            currentIndex: kcm.borderSize
                            onActivated: {
                                kcm.borderSize = currentIndex
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
