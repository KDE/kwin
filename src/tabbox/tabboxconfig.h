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
     * desktops are included in the Window List in the ClientModel
     */
    enum ClientDesktopMode {
        AllDesktopsClients, ///< Windows from all desktops are included.
        OnlyCurrentDesktopClients, ///< Only Windows on current desktop are included
        ExcludeCurrentDesktopClients ///< Exclude Windows on current desktop
    };
    /**
     * ClientActivitiesMode defines whether windows from the current activity or from all
     * activities are included in the Window List in the ClientModel
     */
    enum ClientActivitiesMode {
        AllActivitiesClients, ///< Windows from all Activities are included.
        OnlyCurrentActivityClients, ///< Only Windows on current activity are included
        ExcludeCurrentActivityClients ///< Exclude Windows on current activity
    };
    /**
     * ClientApplicationsMode defines which windows from the current application or from all
     * applications are included in the Window List in the ClientModel
     */
    enum ClientApplicationsMode {
        AllWindowsAllApplications, ///< Windows from all applications are included
        OneWindowPerApplication, ///< Only one Window for each application is included
        AllWindowsCurrentApplication ///< Only Windows for the current application are included
    };
    /**
     * ClientMinimizedMode defines which windows are included in the Window List
     * in the ClientModel based on whether they are minimized or not
     */
    enum ClientMinimizedMode {
        IgnoreMinimizedStatus, ///< Windows are included no matter they are minimized or not
        ExcludeMinimizedClients, ///< Exclude minimized Windows
        OnlyMinimizedClients ///< Only minimized Windows are included
    };
    /**
     * ShowDesktopMode defines whether a Window representing the desktop
     * is included in the Window List in the ClientModel
     */
    enum ShowDesktopMode {
        DoNotShowDesktopClient, ///< A Window representing the desktop is not included
        ShowDesktopClient ///< A Window representing the desktop is included
    };
    /**
     * OrderMinimizedMode defines whether the Windows considered part of the
     * Window List should be grouped by the minimisation status
     */
    enum OrderMinimizedMode {
        NoGroupByMinimized, ///< Windows are not grouped by whether they are minimized
        GroupByMinimized ///< Windows are grouped by whether they are minimized or not
    };
    /**
     * ClientActivitiesMode defines whether windows from the current activity or from all
     * activities are included in the Window List in the ClientModel
     */
    enum ClientMultiScreenMode {
        IgnoreMultiScreen, ///< Windows are included independently of the screen they are on
        OnlyCurrentScreenClients, ///< Only Windows on current screen are included
        ExcludeCurrentScreenClients ///< Exclude Windows from the current screen
    };
    /**
     * ClientSwitchingMode defines the sorting of the Windows in the
     * ClientModel.
     */
    enum ClientSwitchingMode {
        FocusChainSwitching, ///< Sort by recently used. Most recently used Window is the first
        StackingOrderSwitching ///< Sort by current stacking order
    };
    TabBoxConfig();
    ~TabBoxConfig();
    TabBoxConfig &operator=(const TabBoxConfig &object);

    // getters
    /**
     * @return If the TabBox should be shown or not
     * @see setShowTabBox
     * @see defaultShowTabBox
     */
    bool isShowTabBox() const;
    /**
     * @return If Highlight Window effect should be used.
     * @see setHighlightWindows
     * @see defaultHighlightWindows
     */
    bool isHighlightWindows() const;
    /**
     * @return The current ClientDesktopMode
     * @see setClientDesktopMode
     * @see defaultDesktopMode
     */
    ClientDesktopMode clientDesktopMode() const;
    /**
     * @return The current ClientActivitiesMode
     * @see setClientActivitiesMode
     * @see defaultActivitiesMode
     */
    ClientActivitiesMode clientActivitiesMode() const;
    /**
     * @return The current ClientApplicationsMode
     * @see setClientApplicationsMode
     * @see defaultApplicationsMode
     */
    ClientApplicationsMode clientApplicationsMode() const;
    /**
     * @return The current OrderMinimizedMode
     * @see setOrderMinimizedMode
     * @see defaultOrderMinimizedMode
     */
    OrderMinimizedMode orderMinimizedMode() const;
    /**
     * @return The current ClientMinimizedMode
     * @see setClientMinimizedMode
     * @see defaultMinimizedMode
     */
    ClientMinimizedMode clientMinimizedMode() const;
    /**
     * @return The current ShowDesktopMode
     * @see setShowDesktopMode
     * @see defaultShowDesktopMode
     */
    ShowDesktopMode showDesktopMode() const;
    /**
     * @return The current ClientMultiScreenMode
     * @see setClientMultiScreenMode
     * @see defaultMultiScreenMode
     */
    ClientMultiScreenMode clientMultiScreenMode() const;
    /**
     * @return The current ClientSwitchingMode.
     * @see setClientSwitchingMode
     * @see defaultSwitchingMode
     */
    ClientSwitchingMode clientSwitchingMode() const;
    /**
     * @return Then name of the current ItemLayout
     * @see setlayoutName
     */
    QString &layoutName() const;

    // setters
    /**
     * @param show The tabbox should be shown or not.
     * @see isShowTabBox
     */
    void setShowTabBox(bool show);
    /**
     * @param highlight Highlight Windows effect should be used or not.
     * @see isHighlightWindows
     */
    void setHighlightWindows(bool highlight);
    /**
     * @param desktopMode The new ClientDesktopMode to be used.
     * @see clientDesktopMode
     */
    void setClientDesktopMode(ClientDesktopMode desktopMode);
    /**
     * @param activitiesMode The new ClientActivitiesMode to be used.
     * @see clientActivitiesMode
     */
    void setClientActivitiesMode(ClientActivitiesMode activitiesMode);
    /**
     * @param applicationsMode The new ClientApplicationsMode to be used.
     * @see clientApplicationsMode
     */
    void setClientApplicationsMode(ClientApplicationsMode applicationsMode);
    /**
     * @param orderMinimizedMode The new OrderMinimizedMode to be used.
     * @see orderMinimizedMode
     */
    void setOrderMinimizedMode(OrderMinimizedMode orderMinimizedMode);
    /**
     * @param minimizedMode The new ClientMinimizedMode to be used.
     * @see clientMinimizedMode
     */
    void setClientMinimizedMode(ClientMinimizedMode minimizedMode);
    /**
     * @param showDesktopMode The new ShowDesktopMode to be used.
     * @see showDesktopMode
     */
    void setShowDesktopMode(ShowDesktopMode showDesktopMode);
    /**
     * @param multiScreenMode The new ClientMultiScreenMode to be used.
     * @see clientMultiScreenMode
     */
    void setClientMultiScreenMode(ClientMultiScreenMode multiScreenMode);
    /**
     * @param switchingMode The new ClientSwitchingMode to be used.
     * @see clientSwitchingMode
     */
    void setClientSwitchingMode(ClientSwitchingMode switchingMode);
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
        return QStringLiteral("thumbnail_grid");
    }

private:
    TabBoxConfigPrivate *d;
};

} // namespace TabBox
} // namespace KWin
