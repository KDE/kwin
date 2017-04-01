import QtQuick 2.0;
import org.kde.kwin 2.0;

ScreenEdgeItem {
    edge: ScreenEdgeItem.LeftEdge
    mode: ScreenEdgeItem.Touch
    onActivated: {
        workspace.slotToggleShowDesktop();
    }
}
