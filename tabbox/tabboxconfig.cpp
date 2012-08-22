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
        , showOutline(TabBoxConfig::defaultShowOutline())
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
    bool showOutline;

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
    d->showOutline = object.isShowOutline();
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

void TabBoxConfig::setShowOutline(bool show)
{
    d->showOutline = show;
}

bool TabBoxConfig::isShowOutline() const
{
    return d->showOutline;
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
