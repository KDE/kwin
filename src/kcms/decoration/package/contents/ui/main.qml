/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2

import org.kde.kcm 1.6 as KCM
import org.kde.kirigami 2.20 as Kirigami
import org.kde.newstuff 1.85 as NewStuff

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

            // Note: StackLayout does not support neither anchor.margins, not Layout.margins properties.
            StackLayout {
                anchors.fill: parent

                currentIndex: tabBar.currentIndex

                ColumnLayout {
                    spacing: 0

                    Themes {
                        id: themes

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        KCM.SettingStateBinding {
                            target: themes
                            configObject: kcm.settings
                            settingName: "pluginName"
                        }
                    }

                    RowLayout {
                        // default bottom padding of a Frame is fine.
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        // no right margin is OK for button, but bad for text label on the left.
                        Layout.leftMargin: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: i18nc("Selector label", "Window border size:")
                        }

                        QQC2.ComboBox {
                            id: borderSizeComboBox
                            model: kcm.borderSizesModel
                            currentIndex: kcm.borderIndex
                            onActivated: kcm.borderIndex = currentIndex;
                            KCM.SettingHighlighter {
                                highlight: kcm.borderIndex !== 0
                            }
                        }

                        Kirigami.ActionToolBar {
                            flat: false
                            alignment: Qt.AlignRight
                            actions: [
                                NewStuff.Action {
                                    text: i18nc("button text", "Get New Window Decorations...")
                                    configFile: "window-decorations.knsrc"
                                    onEntryEvent: (entry, event) => {
                                        if (event === NewStuff.Engine.StatusChangedEvent) {
                                            kcm.reloadKWinSettings();
                                        } else if (event === NewStuff.Engine.EntryAdoptedEvent) {
                                            kcm.load();
                                        }
                                    }
                                }
                            ]
                        }
                    }
                }

                ColumnLayout {
                    spacing: 0

                    Buttons {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: Kirigami.Units.smallSpacing
                    }

                    ColumnLayout {
                        Layout.margins: Kirigami.Units.smallSpacing

                        QQC2.CheckBox {
                            id: closeOnDoubleClickOnMenuCheckBox
                            text: i18nc("checkbox label", "Close windows by double clicking the menu button")
                            checked: kcm.settings.closeOnDoubleClickOnMenu
                            onToggled: {
                                kcm.settings.closeOnDoubleClickOnMenu = checked;
                                infoLabel.visible = checked;
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
                            text: i18nc("popup tip", "Click and hold on the menu button to show the menu.")
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
}
