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

#include "previewhandlerimpl.h"
#include <kephal/screens.h>
#include <klocalizedstring.h>
#include <kwindowsystem.h>
#include <KDebug>

namespace KWin
{
namespace TabBox
{

PreviewClientImpl::PreviewClientImpl(WId id)
    : m_id(id)
{
}

PreviewClientImpl::~PreviewClientImpl()
{
}

QString PreviewClientImpl::caption() const
{
    KWindowInfo info = KWindowSystem::windowInfo(m_id, NET::WMVisibleName);
    return info.visibleName();
}

QPixmap PreviewClientImpl::icon(const QSize& size) const
{
    return KWindowSystem::icon(m_id, size.width(), size.height(), true);
}

bool PreviewClientImpl::isMinimized() const
{
    KWindowInfo info = KWindowSystem::windowInfo(m_id, NET::WMState | NET::XAWMState);
    return info.isMinimized();
}

int PreviewClientImpl::width() const
{
    return 0; // only needed for the outline - not needed in preview
}

int PreviewClientImpl::height() const
{
    return 0; // only needed for the outline - not needed in preview
}

WId PreviewClientImpl::window() const
{
    return m_id;
}

int PreviewClientImpl::x() const
{
    return 0; // only needed for the outline - not needed in preview
}

int PreviewClientImpl::y() const
{
    return 0; // only needed for the outline - not needed in preview
}

/*******************************************************
* PreviewHandlerImpl
*******************************************************/
PreviewHandlerImpl::PreviewHandlerImpl()
{
    QList< WId > windows = KWindowSystem::stackingOrder();
    foreach (WId w, windows) {
        m_stackingOrder.append(new PreviewClientImpl(w));
        kDebug(1212) << "Window " << w;
    }
}

PreviewHandlerImpl::~PreviewHandlerImpl()
{
    qDeleteAll(m_stackingOrder.begin(), m_stackingOrder.end());
    m_stackingOrder.clear();
}

TabBoxClient* PreviewHandlerImpl::clientToAddToList(TabBoxClient* client, int desktop, bool allDesktops) const
{
    Q_UNUSED(desktop)
    Q_UNUSED(allDesktops)
    // don't include desktops and panels
    KWindowInfo info = KWindowSystem::windowInfo(client->window(), NET::WMWindowType);
    NET::WindowType wType = info.windowType(NET::NormalMask | NET::DesktopMask | NET::DockMask |
                                            NET::ToolbarMask | NET::MenuMask | NET::DialogMask |
                                            NET::OverrideMask | NET::TopMenuMask |
                                            NET::UtilityMask | NET::SplashMask);

    if (wType != NET::Normal && wType != NET::Override && wType != NET::Unknown &&
            wType != NET::Dialog && wType != NET::Utility) {
        return NULL;
    }
    return client;
}

TabBoxClientList PreviewHandlerImpl::stackingOrder() const
{
    return m_stackingOrder;
}

int PreviewHandlerImpl::nextDesktopFocusChain(int desktop) const
{
    int ret = desktop + 1;
    if (ret > numberOfDesktops())
        ret = 1;
    return ret;
}

int PreviewHandlerImpl::numberOfDesktops() const
{
    return KWindowSystem::numberOfDesktops();
}

int PreviewHandlerImpl::currentDesktop() const
{
    return KWindowSystem::currentDesktop();
}

QString PreviewHandlerImpl::desktopName(int desktop) const
{
    return KWindowSystem::desktopName(desktop);
}

QString PreviewHandlerImpl::desktopName(TabBoxClient* client) const
{
    Q_UNUSED(client)
    return desktopName(1);
}

TabBoxClient* PreviewHandlerImpl::nextClientFocusChain(TabBoxClient* client) const
{
    if (m_stackingOrder.isEmpty())
        return NULL;
    int index = m_stackingOrder.indexOf(client);
    index++;
    if (index >= m_stackingOrder.count())
        index = 0;
    return m_stackingOrder[ index ];
}

KWin::TabBox::TabBoxClient* PreviewHandlerImpl::activeClient() const
{
    if (m_stackingOrder.isEmpty())
        return NULL;
    return m_stackingOrder[ 0 ];
}

int PreviewHandlerImpl::activeScreen() const
{
    return 0;
}

TabBoxClient* PreviewHandlerImpl::desktopClient() const
{
    return 0;
}


} // namespace TabBox
} // namespace KWin
