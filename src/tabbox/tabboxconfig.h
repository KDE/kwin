/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

/**
 * @file
 * This file defines the class TabBoxConfig.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
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
 * @author Martin Gräßlin <mgraesslin@kde.org>
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
     * OrderMinimizedMode defines whether the TabBoxClients considered part of the
     * TabBoxClient List should be grouped by the minimisation status
     */
    enum OrderMinimizedMode {
        NoGroupByMinimized, ///< TabBoxClients are not grouped by whether they are minimized
        GroupByMinimized ///< TabBoxClients are grouped by whether they are minimized or not
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
        MostRecentlyUsedDesktopSwitching, ///< Sort by recently used. Most recently used desktop is the first
        StaticDesktopSwitching ///< Static sorting in numerical ascending order
    };
    /**
     * TabBoxMode defines what kind of items the TabBox is displaying and which
     * Model is used
     */
    enum TabBoxMode {
        ClientTabBox, ///< TabBox uses TabBoxClientModel
        DesktopTabBox ///< TabBox uses TabBoxDesktopModel
    };
    TabBoxConfig();
    ~TabBoxConfig();
    TabBoxConfig &operator=(const TabBoxConfig &object);

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
     * @return The current OrderMinimizedMode
     * This option only applies for TabBoxMode ClientTabBox.
     * @see setOrderMinimizedMode
     * @see defaultOrderMinimizedMode
     */
    OrderMinimizedMode orderMinimizedMode() const;
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
    QString &layoutName() const;

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
     * @param orderMinimizedMode The new OrderMinimizedMode to be used.
     * This option only applies for TabBoxMode ClientTabBox.
     * @see orderMinimizedMode
     */
    void setOrderMinimizedMode(OrderMinimizedMode orderMinimizedMode);
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
    void setLayoutName(const QString &name);

    // some static methods to access default values
    static ClientDesktopMode defaultDesktopMode()
    {
        return OnlyCurrentDesktopClients;
    }
    static ClientActivitiesMode defaultActivitiesMode()
    {
        return OnlyCurrentActivityClients;
    }
    static ClientApplicationsMode defaultApplicationsMode()
    {
        return AllWindowsAllApplications;
    }
    static OrderMinimizedMode defaultOrderMinimizedMode()
    {
        return NoGroupByMinimized;
    }
    static ClientMinimizedMode defaultMinimizedMode()
    {
        return IgnoreMinimizedStatus;
    }
    static ShowDesktopMode defaultShowDesktopMode()
    {
        return DoNotShowDesktopClient;
    }
    static ClientMultiScreenMode defaultMultiScreenMode()
    {
        return IgnoreMultiScreen;
    }
    static ClientSwitchingMode defaultSwitchingMode()
    {
        return FocusChainSwitching;
    }
    static bool defaultShowTabBox()
    {
        return true;
    }
    static bool defaultHighlightWindow()
    {
        return true;
    }
    static QString defaultLayoutName()
    {
        return QStringLiteral("org.kde.breeze.desktop");
    }

private:
    TabBoxConfigPrivate *d;
};

} // namespace TabBox
} // namespace KWin
