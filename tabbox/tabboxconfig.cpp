/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabboxconfig.h"

namespace KWin
{
namespace TabBox
{
class TabBoxConfigPrivate
{
public:
    TabBoxConfigPrivate()
        : showTabBox(TabBoxConfig::defaultShowTabBox())
        , highlightWindows(TabBoxConfig::defaultHighlightWindow())
        , tabBoxMode(TabBoxConfig::ClientTabBox)
        , clientDesktopMode(TabBoxConfig::defaultDesktopMode())
        , clientActivitiesMode(TabBoxConfig::defaultActivitiesMode())
        , clientApplicationsMode(TabBoxConfig::defaultApplicationsMode())
        , clientMinimizedMode(TabBoxConfig::defaultMinimizedMode())
        , showDesktopMode(TabBoxConfig::defaultShowDesktopMode())
        , clientMultiScreenMode(TabBoxConfig::defaultMultiScreenMode())
        , clientSwitchingMode(TabBoxConfig::defaultSwitchingMode())
        , desktopSwitchingMode(TabBoxConfig::MostRecentlyUsedDesktopSwitching)
        , layoutName(TabBoxConfig::defaultLayoutName()) {
    }
    ~TabBoxConfigPrivate() {
    }
    bool showTabBox;
    bool highlightWindows;

    TabBoxConfig::TabBoxMode tabBoxMode;
    TabBoxConfig::ClientDesktopMode clientDesktopMode;
    TabBoxConfig::ClientActivitiesMode clientActivitiesMode;
    TabBoxConfig::ClientApplicationsMode clientApplicationsMode;
    TabBoxConfig::ClientMinimizedMode clientMinimizedMode;
    TabBoxConfig::ShowDesktopMode showDesktopMode;
    TabBoxConfig::ClientMultiScreenMode clientMultiScreenMode;
    TabBoxConfig::ClientSwitchingMode clientSwitchingMode;
    TabBoxConfig::DesktopSwitchingMode desktopSwitchingMode;
    QString layoutName;
};

TabBoxConfig::TabBoxConfig()
    : d(new TabBoxConfigPrivate)
{
}

TabBoxConfig::~TabBoxConfig()
{
    delete d;
}

TabBoxConfig& TabBoxConfig::operator=(const KWin::TabBox::TabBoxConfig& object)
{
    d->showTabBox = object.isShowTabBox();
    d->highlightWindows = object.isHighlightWindows();
    d->tabBoxMode = object.tabBoxMode();
    d->clientDesktopMode = object.clientDesktopMode();
    d->clientActivitiesMode = object.clientActivitiesMode();
    d->clientApplicationsMode = object.clientApplicationsMode();
    d->clientMinimizedMode = object.clientMinimizedMode();
    d->showDesktopMode = object.showDesktopMode();
    d->clientMultiScreenMode = object.clientMultiScreenMode();
    d->clientSwitchingMode = object.clientSwitchingMode();
    d->desktopSwitchingMode = object.desktopSwitchingMode();
    d->layoutName = object.layoutName();
    return *this;
}

void TabBoxConfig::setHighlightWindows(bool highlight)
{
    d->highlightWindows = highlight;
}

bool TabBoxConfig::isHighlightWindows() const
{
    return d->highlightWindows;
}

void TabBoxConfig::setShowTabBox(bool show)
{
    d->showTabBox = show;
}

bool TabBoxConfig::isShowTabBox() const
{
    return d->showTabBox;
}

void TabBoxConfig::setTabBoxMode(TabBoxConfig::TabBoxMode mode)
{
    d->tabBoxMode = mode;
}

TabBoxConfig::TabBoxMode TabBoxConfig::tabBoxMode() const
{
    return d->tabBoxMode;
}

TabBoxConfig::ClientDesktopMode TabBoxConfig::clientDesktopMode() const
{
    return d->clientDesktopMode;
}

void TabBoxConfig::setClientDesktopMode(ClientDesktopMode desktopMode)
{
    d->clientDesktopMode = desktopMode;
}

TabBoxConfig::ClientActivitiesMode TabBoxConfig::clientActivitiesMode() const
{
    return d->clientActivitiesMode;
}

void TabBoxConfig::setClientActivitiesMode(ClientActivitiesMode activitiesMode)
{
    d->clientActivitiesMode = activitiesMode;
}

TabBoxConfig::ClientApplicationsMode TabBoxConfig::clientApplicationsMode() const
{
    return d->clientApplicationsMode;
}

void TabBoxConfig::setClientApplicationsMode(ClientApplicationsMode applicationsMode)
{
    d->clientApplicationsMode = applicationsMode;
}

TabBoxConfig::ClientMinimizedMode TabBoxConfig::clientMinimizedMode() const
{
    return d->clientMinimizedMode;
}

void TabBoxConfig::setClientMinimizedMode(ClientMinimizedMode minimizedMode)
{
    d->clientMinimizedMode = minimizedMode;
}

TabBoxConfig::ShowDesktopMode TabBoxConfig::showDesktopMode() const
{
    return d->showDesktopMode;
}

void TabBoxConfig::setShowDesktopMode(ShowDesktopMode showDesktopMode)
{
    d->showDesktopMode = showDesktopMode;
}

TabBoxConfig::ClientMultiScreenMode TabBoxConfig::clientMultiScreenMode() const
{
    return d->clientMultiScreenMode;
}

void TabBoxConfig::setClientMultiScreenMode(ClientMultiScreenMode multiScreenMode)
{
    d->clientMultiScreenMode = multiScreenMode;
}

TabBoxConfig::ClientSwitchingMode TabBoxConfig::clientSwitchingMode() const
{
    return d->clientSwitchingMode;
}

void TabBoxConfig::setClientSwitchingMode(ClientSwitchingMode switchingMode)
{
    d->clientSwitchingMode = switchingMode;
}

TabBoxConfig::DesktopSwitchingMode TabBoxConfig::desktopSwitchingMode() const
{
    return d->desktopSwitchingMode;
}

void TabBoxConfig::setDesktopSwitchingMode(DesktopSwitchingMode switchingMode)
{
    d->desktopSwitchingMode = switchingMode;
}

QString& TabBoxConfig::layoutName() const
{
    return d->layoutName;
}

void TabBoxConfig::setLayoutName(const QString& name)
{
    d->layoutName = name;
}

} // namespace TabBox
} // namespace KWin
