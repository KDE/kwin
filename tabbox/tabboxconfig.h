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
    * ClientDesktopMode defines whether windows from the current desktop or from all
    * desktops are included in the TabBoxClient List in the TabBoxClientModel
    */
    enum ClientDesktopMode {
        AllDesktopsClients, ///< TabBoxClients from all desktops are included.
        OnlyCurrentDesktopClients, ///< Only TabBoxClients on current desktop are included
        ExcludeCurrentDesktopClients ///< Exclude TabBoxClients on current desktop
    };
    /**
    * ClientActivitiesMode defines whether windows from the current activity or from all
    * activities are included in the TabBoxClient List in the TabBoxClientModel
    */
    enum ClientActivitiesMode {
        AllActivitiesClients, ///< TabBoxClients from all Activities are included.
        OnlyCurrentActivityClients, ///< Only TabBoxClients on current activity are included
        ExcludeCurrentActivityClients ///< Exclude TabBoxClients on current activity
    };
    /**
    * ClientApplicationsMode defines which windows from the current application or from all
    * applications are included in the TabBoxClient List in the TabBoxClientModel
    */
    enum ClientApplicationsMode {
        AllWindowsAllApplications, ///< TabBoxClients from all applications are included
        OneWindowPerApplication, ///< Only one TabBoxClient for each application is included
        AllWindowsCurrentApplication ///< Only TabBoxClients for the current application are included
    };
    /**
    * ClientMinimizedMode defines which windows are included in the TabBoxClient List
    * in the TabBoxClientModel based on whether they are minimized or not
    */
    enum ClientMinimizedMode {
        IgnoreMinimizedStatus, ///< TabBoxClients are included no matter they are minimized or not
        ExcludeMinimizedClients, ///< Exclude minimized TabBoxClients
        OnlyMinimizedClients ///< Only minimized TabBoxClients are included
    };
    /**
    * ShowDesktopMode defines whether a TabBoxClient representing the desktop
    * is included in the TabBoxClient List in the TabBoxClientModel
    */
    enum ShowDesktopMode {
        DoNotShowDesktopClient, ///< A TabBoxClient representing the desktop is not included
        ShowDesktopClient ///< A TabBoxClient representing the desktop is included
    };
    /**
    * ClientActivitiesMode defines whether windows from the current activity or from all
    * activities are included in the TabBoxClient List in the TabBoxClientModel
    */
    enum ClientMultiScreenMode {
        IgnoreMultiScreen, ///< TabBoxClients are included independently of the screen they are on
        OnlyCurrentScreenClients, ///< Only TabBoxClients on current screen are included
        ExcludeCurrentScreenClients ///< Exclude TabBoxClients from the current screen
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
    * @return The current ClientDesktopMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientDesktopMode
    * @see defaultDesktopMode
    */
    ClientDesktopMode clientDesktopMode() const;
    /**
    * @return The current ClientActivitiesMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientActivitiesMode
    * @see defaultActivitiesMode
    */
    ClientActivitiesMode clientActivitiesMode() const;
    /**
    * @return The current ClientApplicationsMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientApplicationsMode
    * @see defaultApplicationsMode
    */
    ClientApplicationsMode clientApplicationsMode() const;
    /**
    * @return The current ClientMinimizedMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientMinimizedMode
    * @see defaultMinimizedMode
    */
    ClientMinimizedMode clientMinimizedMode() const;
    /**
    * @return The current ShowDesktopMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setShowDesktopMode
    * @see defaultShowDesktopMode
    */
    ShowDesktopMode showDesktopMode() const;
    /**
    * @return The current ClientMultiScreenMode
    * This option only applies for TabBoxMode ClientTabBox.
    * @see setClientMultiScreenMode
    * @see defaultMultiScreenMode
    */
    ClientMultiScreenMode clientMultiScreenMode() const;
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
    * @return Then name of the current ItemLayout
    * @see setlayoutName
    */
    QString& layoutName() const;

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
    * @param desktopMode The new ClientDesktopMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientDesktopMode
    */
    void setClientDesktopMode(ClientDesktopMode desktopMode);
    /**
    * @param activitiesMode The new ClientActivitiesMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientActivitiesMode
    */
    void setClientActivitiesMode(ClientActivitiesMode activitiesMode);
    /**
    * @param applicationsMode The new ClientApplicationsMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientApplicationsMode
    */
    void setClientApplicationsMode(ClientApplicationsMode applicationsMode);
    /**
    * @param minimizedMode The new ClientMinimizedMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientMinimizedMode
    */
    void setClientMinimizedMode(ClientMinimizedMode minimizedMode);
    /**
    * @param showDesktopMode The new ShowDesktopMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see showDesktopMode
    */
    void setShowDesktopMode(ShowDesktopMode showDesktopMode);
    /**
    * @param multiScreenMode The new ClientMultiScreenMode to be used.
    * This option only applies for TabBoxMode ClientTabBox.
    * @see clientMultiScreenMode
    */
    void setClientMultiScreenMode(ClientMultiScreenMode multiScreenMode);
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
    * @param name The new ItemLayout config name
    * @see layoutName
    */
    void setLayoutName(const QString& name);

    // some static methods to access default values
    static ClientDesktopMode defaultDesktopMode() {
        return OnlyCurrentDesktopClients;
    }
    static ClientActivitiesMode defaultActivitiesMode() {
        return OnlyCurrentActivityClients;
    }
    static ClientApplicationsMode defaultApplicationsMode() {
        return AllWindowsAllApplications;
    }
    static ClientMinimizedMode defaultMinimizedMode() {
        return IgnoreMinimizedStatus;
    }
    static ShowDesktopMode defaultShowDesktopMode() {
        return DoNotShowDesktopClient;
    }
    static ClientMultiScreenMode defaultMultiScreenMode() {
        return IgnoreMultiScreen;
    }
    static ClientSwitchingMode defaultSwitchingMode() {
        return FocusChainSwitching;
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
    static QString defaultLayoutName() {
        return QString("thumbnails");
    }
private:
    TabBoxConfigPrivate* d;
};

} // namespace TabBox
} // namespace KWin

#endif // TABBOXCONFIG_H
