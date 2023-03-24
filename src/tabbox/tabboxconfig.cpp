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
        , clientDesktopMode(TabBoxConfig::defaultDesktopMode())
        , clientActivitiesMode(TabBoxConfig::defaultActivitiesMode())
        , clientApplicationsMode(TabBoxConfig::defaultApplicationsMode())
        , orderMinimizedMode(TabBoxConfig::defaultOrderMinimizedMode())
        , clientMinimizedMode(TabBoxConfig::defaultMinimizedMode())
        , showDesktopMode(TabBoxConfig::defaultShowDesktopMode())
        , clientMultiScreenMode(TabBoxConfig::defaultMultiScreenMode())
        , clientSwitchingMode(TabBoxConfig::defaultSwitchingMode())
        , layoutName(TabBoxConfig::defaultLayoutName())
    {
    }
    ~TabBoxConfigPrivate()
    {
    }
    bool showTabBox;
    bool highlightWindows;

    TabBoxConfig::ClientDesktopMode clientDesktopMode;
    TabBoxConfig::ClientActivitiesMode clientActivitiesMode;
    TabBoxConfig::ClientApplicationsMode clientApplicationsMode;
    TabBoxConfig::OrderMinimizedMode orderMinimizedMode;
    TabBoxConfig::ClientMinimizedMode clientMinimizedMode;
    TabBoxConfig::ShowDesktopMode showDesktopMode;
    TabBoxConfig::ClientMultiScreenMode clientMultiScreenMode;
    TabBoxConfig::ClientSwitchingMode clientSwitchingMode;
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

TabBoxConfig &TabBoxConfig::operator=(const KWin::TabBox::TabBoxConfig &object)
{
    d->showTabBox = object.isShowTabBox();
    d->highlightWindows = object.isHighlightWindows();
    d->clientDesktopMode = object.clientDesktopMode();
    d->clientActivitiesMode = object.clientActivitiesMode();
    d->clientApplicationsMode = object.clientApplicationsMode();
    d->orderMinimizedMode = object.orderMinimizedMode();
    d->clientMinimizedMode = object.clientMinimizedMode();
    d->showDesktopMode = object.showDesktopMode();
    d->clientMultiScreenMode = object.clientMultiScreenMode();
    d->clientSwitchingMode = object.clientSwitchingMode();
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

TabBoxConfig::OrderMinimizedMode TabBoxConfig::orderMinimizedMode() const
{
    return d->orderMinimizedMode;
}

void TabBoxConfig::setOrderMinimizedMode(OrderMinimizedMode orderMinimizedMode)
{
    d->orderMinimizedMode = orderMinimizedMode;
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

QString &TabBoxConfig::layoutName() const
{
    return d->layoutName;
}

void TabBoxConfig::setLayoutName(const QString &name)
{
    d->layoutName = name;
}

} // namespace TabBox
} // namespace KWin
