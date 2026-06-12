import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.overlay

Window {
    id: root
    color: "transparent"

    Rectangle {
        anchors.centerIn: parent
        width: 160 * 5
        height: 90 * 5
        color: "#800000FF"

        radius: 10
        border.color: "black"
        border.width: 10

        Controls.Label {
            anchors.centerIn: parent
            text: "Hello, Overlay!"
        }
    }

    Component.onCompleted: {
        OverlayShellIntegration.assignOverlayRole(root);
        show();
    }
}
