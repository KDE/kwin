/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_output.h"
#include "screens.h"

namespace KWin
{

X11Output::X11Output(QObject *parent)
    : AbstractOutput(parent)
{
}

QString X11Output::name() const
{
    return m_name;
}

void X11Output::setName(QString set)
{
    m_name = set;
}

QRect X11Output::geometry() const
{
    if (m_geometry.isValid()) {
        return m_geometry;
    }
    return QRect(QPoint(0, 0), Screens::self()->displaySize()); // xinerama, lacks RandR
}

void X11Output::setGeometry(QRect set)
{
    m_geometry = set;
}

int X11Output::refreshRate() const
{
    return m_refreshRate;
}

void X11Output::setRefreshRate(int set)
{
    m_refreshRate = set;
}

int X11Output::gammaRampSize() const
{
    return m_gammaRampSize;
}

bool X11Output::setGammaRamp(const GammaRamp &gamma)
{
    if (m_crtc == XCB_NONE) {
        return false;
    }

    xcb_randr_set_crtc_gamma(connection(), m_crtc, gamma.size(), gamma.red(),
        gamma.green(), gamma.blue());

    return true;
}

void X11Output::setCrtc(xcb_randr_crtc_t crtc)
{
    m_crtc = crtc;
}

void X11Output::setGammaRampSize(int size)
{
    m_gammaRampSize = size;
}

QSize X11Output::physicalSize() const
{
    return m_physicalSize;
}

void X11Output::setPhysicalSize(const QSize &size)
{
    m_physicalSize = size;
}

QSize X11Output::pixelSize() const
{
    return geometry().size();
}

}
