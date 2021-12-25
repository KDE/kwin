/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_output.h"
#include "main.h"

namespace KWin
{

X11Output::X11Output(const QString &name, QObject *parent)
    : AbstractOutput(parent)
    , m_name(name)
{
}

QString X11Output::name() const
{
    return m_name;
}

int X11Output::xineramaNumber() const
{
    return m_xineramaNumber;
}

void X11Output::setXineramaNumber(int number)
{
    m_xineramaNumber = number;
}

QRect X11Output::geometry() const
{
    return m_geometry;
}

void X11Output::setGeometry(QRect set)
{
    if (m_geometry != set) {
        m_geometry = set;
        Q_EMIT geometryChanged();
    }
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

    xcb_randr_set_crtc_gamma(kwinApp()->x11Connection(), m_crtc, gamma.size(), gamma.red(),
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

bool X11Output::usesSoftwareCursor() const
{
    return false;
}

}
