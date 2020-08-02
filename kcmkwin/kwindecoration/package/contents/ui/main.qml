/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as Controls
import org.kde.kcm 1.1 as KCM
import org.kde.kconfig 1.0 // for KAuthorized
import org.kde.kirigami 2.4 as Kirigami

Kirigami.Page {
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

        Controls.TabBar {
            id: tabBar
            // Tab styles generally assume that they're touching the inner layout,
            // not the frame, so we need to move the tab bar down a pixel and make
            // sure it's drawn on top of the frame
            z: 1
            Layout.bottomMargin: -1
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
                        enabled: !kcm.settings.isImmutable("pluginName") && !kcm.settings.isImmutable("theme")
                    }

                    RowLayout {
                        Controls.CheckBox {
                            id: borderSizeAutoCheckbox
                            // Let it elide but don't make it push the ComboBox away from it
                            Layout.fillWidth: true
                            Layout.maximumWidth: implicitWidth
                            text: i18nc("checkbox label", "Use theme's default window border size")
                            enabled: !kcm.settings.isImmutable("borderSizeAuto")
                            checked: kcm.settings.borderSizeAuto
                            onToggled: {
                                kcm.settings.borderSizeAuto = checked;
                                borderSizeComboBox.autoBorderUpdate()
                            }
                        }
                        Controls.ComboBox {
                            id: borderSizeComboBox
                            enabled: !borderSizeAutoCheckbox.checked && !kcm.settings.isImmutable("borderSize")
                            model: kcm.borderSizesModel
                            currentIndex: kcm.borderSize
                            onActivated: {
                                kcm.borderSize = currentIndex
                            }
                            function autoBorderUpdate() {
                                if (borderSizeAutoCheckbox.checked) {
                                    kcm.borderSize = kcm.recommendedBorderSize
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
                        enabled: !kcm.settings.isImmutable("buttonsOnLeft") && !kcm.settings.isImmutable("buttonsOnRight")
                    }

                    Controls.CheckBox {
                        id: closeOnDoubleClickOnMenuCheckBox
                        text: i18nc("checkbox label", "Close windows by double clicking the menu button")
                        enabled: !kcm.settings.isImmutable("closeOnDoubleClickOnMenu")
                        checked: kcm.settings.closeOnDoubleClickOnMenu
                        onToggled: {
                            kcm.settings.closeOnDoubleClickOnMenu = checked
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
                        enabled: !kcm.settings.isImmutable("showToolTips")
                        checked: kcm.settings.showToolTips
                        onToggled: kcm.settings.showToolTips = checked
                    }
                }
            }
        }
    }
}
