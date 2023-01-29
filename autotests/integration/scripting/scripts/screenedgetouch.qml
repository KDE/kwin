import QtQuick 2.0;
import org.kde.kwin 3.0;

ScreenEdgeItem {
    edge: ScreenEdgeItem.LeftEdge
    mode: ScreenEdgeItem.Touch
    onActivated: {
        Workspace.slotToggleShowDesktop();
    }
}
