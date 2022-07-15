pragma Singleton
import QtQuick 2.15

QtObject {
    property int currentDesktop : 1
    property var currentVirtualDesktop : null
    property var activeClient : null
    property size desktopGridSize : Qt.size(1,3)
    property int desktopGridWidth : 1
    property int desktopGridHeight : 3
    property int workspaceWidth : 1024
    property int workspaceHeight : 2304
    property size workspaceSize : Qt.size(1024, 2304)
    property int desktops : 3
    property size displaySize : Qt.size(1024, 768)
    property int displayWidth : 1024
    property int displayHeight : 768
    property int activeScreen : 0
    property int numScreens : 1
    property string currentActivity : "f14912cd-330b-4891-89ab-63095511a56c"
    property var activities : ["f14912cd-330b-4891-89ab-63095511a56c"]
    property size virtualScreenSize : Qt.size(1024, 2304)
    property rect virtualScreenGeometry : Qt.rect(0,0, 1024, 2304)
    property list<Window> clients : [
        Window {
            alpha : false
            frameId : 106954774
            size: Qt.size(1024, 768)
            x : 0
            y : 0
            width : 1024
            height : 768
            opacity : 1
            screen : 0
            windowId : 18874379
            resourceName : "plasmashell"
            resourceClass : "plasmashell"
            desktopWindow : true
            dock : false
            toolbar : false
            menu : false
            normalWindow : false
            dialog : false
            splash : false
            utility : false
            dropdownMenu : false
            popupMenu : false
            tooltip : false
            notification : false
            criticalNotification : false
            appletPopup : false
            onScreenDisplay : false
            comboBox : false
            dndIcon : false
            windowType : 1
            managed : true
            deleted : false
            shaped : false
            skipsCloseAnimation : false
            popupWindow : false
            outline : false
            internalId : "{65992c6e-01d3-4b1a-9ea0-8e9061c487fa}"
            pid : 1067
            stackingOrder : 0
            fullScreen : false
            fullScreenable : false
            active : false
            desktop : -1
            onAllDesktops : true
            skipTaskbar : false
            skipPager : false
            skipSwitcher : false
            closeable : false
            keepAbove : false
            keepBelow : true
            shadeable : false
            shade : false
            minimizable : false
            minimized : false
            specialWindow : true
            demandsAttention : false
            caption : "Desktop — Plasma"
            wantsInput : true
            modal : false
            move : false
            resize : false
            decorationHasAlpha : false
            noBorder : true
            providesContextHelp : false
            maximizable : false
            moveable : false
            moveableAcrossScreens : false
            resizeable : false
            desktopFileName : "org.kde.plasmashell"
            hasApplicationMenu : false
            applicationMenuActive : false
            unresponsive : false
            colorScheme : "kdeglobals"
            layer : 0
            hidden : false
            blocksCompositing : false
            clientSideDecorated : false
        },
        Window {
            alpha : true
            frameId : 106954781
            x : -24
            y : 277
            width : 1023
            height : 76
            opacity : 1
            screen : 0
            windowId : 52428807
            resourceName : "konsole"
            resourceClass : "konsole"
            windowRole : "MainWindow#1"
            desktopWindow : false
            dock : false
            toolbar : false
            menu : false
            normalWindow : true
            dialog : false
            splash : false
            utility : false
            dropdownMenu : false
            popupMenu : false
            tooltip : false
            notification : false
            criticalNotification : false
            appletPopup : false
            onScreenDisplay : false
            comboBox : false
            dndIcon : false
            windowType : 0
            managed : true
            deleted : false
            shaped : false
            skipsCloseAnimation : false
            popupWindow : false
            outline : false
            internalId : "{6aea6c53-8aa8-4c81-97df-268681997f6e}"
            pid : 1249
            stackingOrder : 2
            fullScreen : false
            fullScreenable : true
            active : false
            desktop : 2
            onAllDesktops : false
            skipTaskbar : false
            skipPager : false
            skipSwitcher : false
            closeable : true
            keepAbove : false
            keepBelow : false
            shadeable : true
            shade : false
            minimizable : true
            minimized : false
            specialWindow : false
            demandsAttention : false
            caption : "ksmserver : zsh — Konsole"
            wantsInput : true
            modal : false
            move : false
            resize : false
            decorationHasAlpha : true
            noBorder : false
            providesContextHelp : false
            maximizable : true
            moveable : true
            moveableAcrossScreens : true
            resizeable : true
            desktopFileName : "org.kde.konsole"
            hasApplicationMenu : false
            applicationMenuActive : false
            unresponsive : false
            layer : 2
            hidden : false
            blocksCompositing : false
            clientSideDecorated : false
        }
    ]
}
