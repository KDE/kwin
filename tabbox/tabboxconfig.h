/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef TABBOXCONFIG_H
#define TABBOXCONFIG_H

#include <QString>

/**
* @file
* This file defines the class TabBoxConfig.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/

namespace KWin
{
namespace TabBox
{
class TabBoxConfigPrivate;

/**
* The TabBoxConfig class holds all configuration options for the TabBox.
* The TabBoxHandler contains a pointer to an object of this class and is
* used by all classes of TabBox. The config defines what kind of data the
* TabBox is displaying and how the layout looks like. There can be different
* Config sets and by setting a new config in the TabBoxHandler the behaviour
* of the TabBox is changed immediately.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxConfig
{
public:
    /**
    * LayoutMode defines how the items will be displayed in the
    * main TableView of the TabBox.
    */
    enum LayoutMode {
        VerticalLayout, ///< Items are laid out vertically
        HorizontalLayout, ///< Items are laid out horizontally
        HorizontalVerticalLayout ///< Items are laid out in a tabular. Number of columns might be greater by one than number of rows
    };
    /**
    * ClientListMode defines the mode used to create the TabBoxClient List
    * in the TabBoxClientModel
    */
    enum ClientListMode {
        CurrentDesktopClientList, ///< Only TabBoxClients on current desktop are included
        AllDesktopsClientList, ///< TabBoxClients from all desktops are included.
        CurrentDesktopApplicationList, ///< Only one TabBoxClient for each application on current desktop is included
        AllDesktopsApplicationList ///< Only one TabBoxClient for each application is included
    };
    /**
    * ClientSwitchingMode defines the sorting of the TabBoxClients in the
    * TabBoxClientModel.
    */
    enum ClientSwitchingMode {
        FocusChainSwitching, ///< Sort by recently used. Most recently used TabBoxClient is the first
        StackingOrderSwitching ///< Sort by current stacking order
    };
    /**
    * DesktopSwitchingMode defines the sorting of the desktops in the
    * TabBoxDesktopModel.
    */
    enum DesktopSwitchingMode {
        MostRecentlyUsedDesktopSwitching,///< Sort by recently used. Most recently used desktop is the first
        StaticDesktopSwitching///< Static sorting in numerical ascending order
    };
    /**
    * TabBoxMode defines what kind of items the TabBox is displaying and which
    * Model is used
    */
    enum TabBoxMode {
        ClientTabBox,///< TabBox uses TabBoxClientModel
        DesktopTabBox///< TabBox uses TabBoxDesktopModel
    };
    /**
    * SelectedItemViewPosition defines where an additional view only showing the selected item is shown.
    * This second view is useful when using for example icon only layout and wanting to provide the
    * caption of selected item.
    */
    enum SelectedItemViewPosition {
        NonePosition,///< There is no additional view for selected item
        AbovePosition,///< Additional view is positioned above of the switching list
        BelowPosition,///< Additional view is positioned below of the switching list
        LeftPosition,///< Additional view is positioned left of the switching list
        RightPosition///< Additional view is positioned right of the switching list
    };
    TabBoxConfig();
    ~TabBoxConfig();
    TabBoxConfig& operator=(const TabBoxConfig& object);

    // getters
    /**
    * @return If the TabBox should be shown or not
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see setShowTabBox
    * @see defaultShowTabBox
    */
    bool isShowTabBox() const;
    /**
    * @return If Highlight Window effect should be used.
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see setHighlightWindows
    * @see defaultHighlightWindows
    */
    bool isHighlightWindows() const;
    /**
    * @return If an outline of selected window should be shown
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see setShowOutline
    * @see defaultShowOutline
    */
    bool isShowOutline() const;
    /**
    * @return The current TabBoxMode
    * @see setTabBoxMode
    */
    TabBoxMode tabBoxMode() const;
    /**
    * @return The currently used layout
    * @see setLayout
    * @see defaultLayoutMode
    */
    LayoutMode layout() const;
    /**
    * @return The current ClientListMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientListMode
    * @see defaultListMode
    */
    ClientListMode clientListMode() const;
    /**
    * @return The current ClientSwitchingMode.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientSwitchingMode
    * @see defaultSwitchingMode
    */
    ClientSwitchingMode clientSwitchingMode() const;
    /**
    * @return The current DesktopSwitchingMode
    * This option only applies for TabBoxMode DesktopTabBox.
    * @see setDesktopSwitchingMode
    */
    DesktopSwitchingMode desktopSwitchingMode() const;
    /**
    * @return The position of the selected item view.
    * @see setSelectedItemViewPosition
    * @see defaultSelectedItemViewPosition
    */
    SelectedItemViewPosition selectedItemViewPosition() const;
    /**
    * @return The minimum width in percent of screen width the TabBox should use.
    * @see setMinWidth
    * @see minHeight
    * @see defaultMinWidth
    */
    int minWidth() const;
    /**
    * @return The minimum height in percent of screen height the TabBox should use.
    * @see setMinHeight
    * @see minWidth
    * @see defaultMinHeight
    */
    int minHeight() const;
    /**
    * @return Then name of the current ItemLayout
    * @see setlayoutName
    */
    QString& layoutName() const;
    /**
    * @return Then name of the current ItemLayout for selected Item view
    * @see setlayoutName
    */
    QString& selectedItemLayoutName() const;
    /**
    * @return If an item for minimizing all windows to the desktop should be included.
    * @see setShowDesktop
    */
    bool isShowDesktop() const;

    // setters
    /**
    * @param show The tabbox should be shown or not.
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see isShowTabBox
    */
    void setShowTabBox(bool show);
    /**
    * @param highlight Highlight Windows effect should be used or not.
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see isHighlightWindows
    */
    void setHighlightWindows(bool highlight);
    /**
    * @param show Show outline for current selected client.
    * This option does not apply for TabBoxMode DesktopTabBox.
    * @see isShowOutline
    */
    void setShowOutline(bool show);
    /**
    * @param mode The new TabBoxMode to be used.
    * @see tabBoxMode
    */
    void setTabBoxMode(TabBoxMode mode);
    /**
    * @param layout The new LayoutMode to be used.
    * @see layout
    */
    void setLayout(LayoutMode layout);
    /**
    * @param listMode The new ClientListMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientListMode
    */
    void setClientListMode(ClientListMode listMode);
    /**
    * @param switchingMode The new ClientSwitchingMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientSwitchingMode
    */
    void setClientSwitchingMode(ClientSwitchingMode switchingMode);
    /**
    * @param switchingMode The new DesktopSwitchingMode to be used.
    * This option only applies for TabBoxMode DesktopTabBox.
    * @see desktopSwitchingMode
    */
    void setDesktopSwitchingMode(DesktopSwitchingMode switchingMode);
    /**
    * @param viewPosition The new position of the selected item view
    * @see selectedItemViewPosition
    */
    void setSelectedItemViewPosition(SelectedItemViewPosition viewPosition);
    /**
    * @param value The minimum width of TabBox in percent of screen width.
    * @see minWidth
    */
    void setMinWidth(int value);
    /**
    * @param value The minimum height of TabBox in percent of screen height.
    * @see minHeight
    */
    void setMinHeight(int value);
    /**
    * @param name The new ItemLayout config name
    * @see layoutName
    */
    void setLayoutName(const QString& name);
    /**
    * @param name The new ItemLayout config name for the selected item view
    * @see selectedItemLayoutName
    */
    void setSelectedItemLayoutName(const QString& name);
    /**
    * @param show Include item to minimize all windows to the desktop
    * @see isShowDesktop
    */
    void setShowDesktop(bool show);

    // some static methods to access default values
    static ClientListMode defaultListMode() {
        return CurrentDesktopClientList;
    }
    static ClientSwitchingMode defaultSwitchingMode() {
        return FocusChainSwitching;
    }
    static LayoutMode defaultLayoutMode() {
        return VerticalLayout;
    }
    static SelectedItemViewPosition defaultSelectedItemViewPosition() {
        return NonePosition;
    }
    static bool defaultShowTabBox() {
        return true;
    }
    static bool defaultShowOutline() {
        return true;
    }
    static bool defaultHighlightWindow() {
        return true;
    }
    static int defaultMinWidth() {
        return 20;
    }
    static int defaultMinHeight() {
        return 20;
    }
    static QString defaultLayoutName() {
        return QString("thumbnails");
    }
    static QString defaultSelectedItemLayoutName() {
        return QString("Text");
    }
    static bool defaultShowDesktop() {
        return false;
    }
private:
    TabBoxConfigPrivate* d;
};

} // namespace TabBox
} // namespace KWin

#endif // TABBOXCONFIG_H
