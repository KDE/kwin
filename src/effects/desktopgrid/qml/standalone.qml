import QtQuick 2.12

Rectangle {
    color: "black"
    width: 1024
    height: 768
    Component {
        id: main
        Main {
            id: root
            targetScreen: QtObject{
                property rect geometry: Qt.rect(0, 0, root.width, root.height)
                property real devicePixelRatio: 1
                property string name: "Screen 0"
                property real refreshRate: 60
            }
            effect: QtObject {
                property int gridRows: 1
                property int gridColumns: 1
                property int animationDuration: 100
                property int layout: 1
                property real partialActivationFactor: 0
                property bool gestureInProgress: false
                property bool showAddRemove: true
                property int desktopNameAlignment
                property int desktopLayoutMode
                property int customLayoutRows
                signal itemDraggedOutOfScreen
                signal itemDroppedOutOfScreen
                function checkItemDraggedOutOfScreen(unusedPoint, unusedItem) {
                    return false;
                }
            }
        }
    }

    Loader {
        id: myLoader
//         active:false
        anchors.fill: parent
        sourceComponent: main
    }
}
