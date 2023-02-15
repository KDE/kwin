import QtQuick 2.0;
import org.kde.kwin 3.0;

ScreenEdgeHandler {
    edge: ScreenEdgeHandler.LeftEdge
    mode: ScreenEdgeHandler.Touch
    onActivated: {
        Workspace.slotToggleShowDesktop();
    }
}
