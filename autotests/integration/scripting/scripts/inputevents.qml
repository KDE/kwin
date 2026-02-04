import QtQuick 2.15
import org.kde.kwin 3.0

Item {
    id: root
    // This script listens to input events and forwards them to the TestProbe
    // object injected from the test via the shared QML context.

    Connections {
        target: InputEvents
        function onKeyPressed(event) { TestProbe.keyPressed(); }
        function onKeyReleased(event) { TestProbe.keyReleased(); }
        function onMouseMoved(event) { TestProbe.mouseMoved(); }
        function onMousePressed(event) { TestProbe.mousePressed(); }
        function onMouseReleased(event) { TestProbe.mouseReleased(); }
    }
}

