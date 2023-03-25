import QtQuick
import org.kde.kwin

ScreenEdgeHandler {
    edge: ScreenEdgeHandler.LeftEdge
    mode: ScreenEdgeHandler.Touch
    onActivated: {
        Workspace.slotToggleShowDesktop();
    }
}
