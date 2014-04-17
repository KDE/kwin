/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens_xrandr.h"
#include "xcbutils.h"

namespace KWin
{

XRandRScreens::XRandRScreens(QObject *parent)
    : Screens(parent)
    , X11EventFilter(Xcb::Extensions::self()->randrNotifyEvent())
{
}

XRandRScreens::~XRandRScreens() = default;

template <typename T>
void XRandRScreens::update()
{
    auto fallback = [this]() {
        m_geometries << QRect();
        setCount(1);
    };
    m_geometries.clear();
    T resources(rootWindow());
    if (resources.isNull()) {
        fallback();
        return;
    }
    xcb_randr_crtc_t *crtcs = resources.crtcs();

    QVector<Xcb::RandR::CrtcInfo> infos(resources->num_crtcs);
    for (int i = 0; i < resources->num_crtcs; ++i) {
        infos[i] = Xcb::RandR::CrtcInfo(crtcs[i], resources->config_timestamp);
    }
    for (int i = 0; i < resources->num_crtcs; ++i) {
        Xcb::RandR::CrtcInfo info(infos.at(i));
        const QRect geo = info.rect();
        if (geo.isValid()) {
            m_geometries << geo;
        }
    }
    if (m_geometries.isEmpty()) {
        fallback();
        return;
    }

    setCount(m_geometries.count());
}


void XRandRScreens::init()
{
    KWin::Screens::init();
    // we need to call ScreenResources at least once to be able to use current
    update<Xcb::RandR::ScreenResources>();
    emit changed();
}

QRect XRandRScreens::geometry(int screen) const
{
    if (screen >= m_geometries.size() || screen < 0) {
        return QRect();
    }
    return m_geometries.at(screen);
}

int XRandRScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    for (int i = 0; i < m_geometries.size(); ++i) {
        const QRect &geo = m_geometries.at(i);
        if (geo.contains(pos)) {
            return i;
        }
        int distance = QPoint(geo.topLeft() - pos).manhattanLength();
        distance = qMin(distance, QPoint(geo.topRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomLeft() - pos).manhattanLength());
        if (distance < minDistance) {
            minDistance = distance;
            bestScreen = i;
        }
    }
    return bestScreen;
}

QSize XRandRScreens::size(int screen) const
{
    const QRect geo = geometry(screen);
    if (!geo.isValid()) {
        return QSize();
    }
    return geo.size();
}

void XRandRScreens::updateCount()
{
    update<Xcb::RandR::CurrentResources>();
}

bool XRandRScreens::event(xcb_generic_event_t *event)
{
    Q_ASSERT((event->response_type & ~0x80) == Xcb::Extensions::self()->randrNotifyEvent());
    // let's try to gather a few XRandR events, unlikely that there is just one
    startChangedTimer();
    return false;
}

} // namespace
