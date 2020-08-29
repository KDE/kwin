import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4 as Controls
import org.kde.kcm 1.1 as KCM
import org.kde.kconfig 1.0 // for KAuthorized
import org.kde.kirigami 2.4 as Kirigami

KCM.SimpleKCM {
    title: i18n("Configure Buttons")

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
