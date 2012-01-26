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
        , layout(TabBoxConfig::defaultLayoutMode())
        , clientListMode(TabBoxConfig::defaultListMode())
        , clientSwitchingMode(TabBoxConfig::defaultSwitchingMode())
        , clientMinimizedMode(TabBoxConfig::defaultMinimizedMode())
        , desktopSwitchingMode(TabBoxConfig::MostRecentlyUsedDesktopSwitching)
        , minWidth(TabBoxConfig::defaultMinWidth())
        , minHeight(TabBoxConfig::defaultMinHeight())
        , layoutName(TabBoxConfig::defaultLayoutName())
        , selectedItemLayoutName(TabBoxConfig::defaultSelectedItemLayoutName())
        , showDesktop(TabBoxConfig::defaultShowDesktop()) {
    }
    ~TabBoxConfigPrivate() {
    }
    bool showTabBox;
    bool highlightWindows;
    bool showOutline;

    TabBoxConfig::TabBoxMode tabBoxMode;
    TabBoxConfig::LayoutMode layout;
    TabBoxConfig::ClientListMode clientListMode;
    TabBoxConfig::ClientSwitchingMode clientSwitchingMode;
    TabBoxConfig::ClientMinimizedMode clientMinimizedMode;
    TabBoxConfig::DesktopSwitchingMode desktopSwitchingMode;
    int minWidth;
    int minHeight;
    QString layoutName;
    QString selectedItemLayoutName;
    bool showDesktop;
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
    d->showDesktop = object.isShowDesktop();
    d->layout = object.layout();
    d->clientListMode = object.clientListMode();
    d->clientSwitchingMode = object.clientSwitchingMode();
    d->clientMinimizedMode = object.clientMinimizedMode();
    d->desktopSwitchingMode = object.desktopSwitchingMode();
    d->selectedItemLayoutName = object.selectedItemLayoutName();
    d->minWidth = object.minWidth();
    d->minHeight = object.minHeight();
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

void TabBoxConfig::setLayout(TabBoxConfig::LayoutMode layout)
{
    d->layout = layout;
}

TabBoxConfig::LayoutMode TabBoxConfig::layout() const
{
    return d->layout;
}

TabBoxConfig::ClientListMode TabBoxConfig::clientListMode() const
{
    return d->clientListMode;
}

void TabBoxConfig::setClientListMode(ClientListMode listMode)
{
    d->clientListMode = listMode;
}

TabBoxConfig::ClientSwitchingMode TabBoxConfig::clientSwitchingMode() const
{
    return d->clientSwitchingMode;
}

void TabBoxConfig::setClientSwitchingMode(ClientSwitchingMode switchingMode)
{
    d->clientSwitchingMode = switchingMode;
}

TabBoxConfig::ClientMinimizedMode TabBoxConfig::clientMinimizedMode() const
{
    return d->clientMinimizedMode;
}

void TabBoxConfig::setClientMinimizedMode(ClientMinimizedMode minimizedMode)
{
    d->clientMinimizedMode = minimizedMode;
}

TabBoxConfig::DesktopSwitchingMode TabBoxConfig::desktopSwitchingMode() const
{
    return d->desktopSwitchingMode;
}

void TabBoxConfig::setDesktopSwitchingMode(DesktopSwitchingMode switchingMode)
{
    d->desktopSwitchingMode = switchingMode;
}

int TabBoxConfig::minWidth() const
{
    return d->minWidth;
}

void TabBoxConfig::setMinWidth(int value)
{
    d->minWidth = value;
}

int TabBoxConfig::minHeight() const
{
    return d->minHeight;
}

void TabBoxConfig::setMinHeight(int value)
{
    d->minHeight = value;
}

QString& TabBoxConfig::layoutName() const
{
    return d->layoutName;
}

void TabBoxConfig::setLayoutName(const QString& name)
{
    d->layoutName = name;
}

QString& TabBoxConfig::selectedItemLayoutName() const
{
    return d->selectedItemLayoutName;
}

void TabBoxConfig::setSelectedItemLayoutName(const QString& name)
{
    d->selectedItemLayoutName = name;
}

bool TabBoxConfig::isShowDesktop() const
{
    return d->showDesktop;
}

void TabBoxConfig::setShowDesktop(bool show)
{
    d->showDesktop = show;
}

} // namespace TabBox
} // namespace KWin
