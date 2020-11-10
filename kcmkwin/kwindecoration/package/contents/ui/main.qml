/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as QQC2

import org.kde.kcm 1.5 as KCM
import org.kde.kconfig 1.0 // for KAuthorized
import org.kde.kirigami 2.4 as Kirigami

Kirigami.Page {
    id: root

    KCM.ConfigModule.quickHelp: i18n("This module lets you configure the window decorations.")
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

    implicitWidth: Kirigami.Units.gridUnit * 48
    implicitHeight: Kirigami.Units.gridUnit * 33

    // TODO: replace this TabBar-plus-Frame-in-a-ColumnLayout with whatever shakes
    // out of https://bugs.kde.org/show_bug.cgi?id=394296
    ColumnLayout {
        id: tabLayout
        anchors.fill: parent
        spacing: 0

        QQC2.TabBar {
            id: tabBar
            // Tab styles generally assume that they're touching the inner layout,
            // not the frame, so we need to move the tab bar down a pixel and make
            // sure it's drawn on top of the frame
            z: 1
            Layout.bottomMargin: -1
            Layout.fillWidth: true

            QQC2.TabButton {
                text: i18nc("tab label", "Theme")
            }

            QQC2.TabButton {
                text: i18nc("tab label", "Titlebar Buttons")
            }
        }
        QQC2.Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                anchors.fill: parent

                currentIndex: tabBar.currentIndex

                Item {
                    KCM.SettingStateBinding {
                        target: themes
                        configObject: kcm.settings
                        settingName: "pluginName"
                    }

                    ColumnLayout {
                        anchors.fill: parent

                        Themes {
                            id: themes
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        RowLayout {
                            QQC2.Label {
                                text: i18nc("Selector label", "Window border size:")
                            }
                            QQC2.ComboBox {
                                id: borderSizeComboBox
                                model: kcm.borderSizesModel
                                currentIndex: kcm.borderIndex
                                onActivated: {
                                    kcm.borderIndex = currentIndex
                                }
                                KCM.SettingHighlighter {
                                    highlight: kcm.borderIndex != 0
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            QQC2.Button {
                                text: i18nc("button text", "Get New Window Decorations...")
                                icon.name: "get-hot-new-stuff"
                                onClicked: kcm.getNewStuff(this)
                                visible: KAuthorized.authorize("ghns")
                            }
                        }
                    }
                }

                ColumnLayout {
                    Buttons {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: Kirigami.Units.smallSpacing
                    }

                    QQC2.CheckBox {
                        id: closeOnDoubleClickOnMenuCheckBox
                        text: i18nc("checkbox label", "Close windows by double clicking the menu button")
                        checked: kcm.settings.closeOnDoubleClickOnMenu
                        onToggled: {
                            kcm.settings.closeOnDoubleClickOnMenu = checked
                            infoLabel.visible = checked
                        }

                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "closeOnDoubleClickOnMenu"
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

                    QQC2.CheckBox {
                        id: showToolTipsCheckBox
                        text: i18nc("checkbox label", "Show titlebar button tooltips")
                        checked: kcm.settings.showToolTips
                        onToggled: kcm.settings.showToolTips = checked

                        KCM.SettingStateBinding {
                            configObject: kcm.settings
                            settingName: "showToolTips"
                        }
                    }
                }
            }
        }
    }
}
