/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_output.h"
#include "core/colorlut.h"
#include "x11_standalone_backend.h"

namespace KWin
{

X11Output::X11Output(X11StandaloneBackend *backend, QObject *parent)
    : Output(parent)
    , m_backend(backend)
{
}

RenderLoop *X11Output::renderLoop() const
{
    return m_loop;
}

void X11Output::setRenderLoop(RenderLoop *loop)
{
    m_loop = loop;
}

int X11Output::xineramaNumber() const
{
    return m_xineramaNumber;
}

void X11Output::setXineramaNumber(int number)
{
    m_xineramaNumber = number;
}

bool X11Output::setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation)
{
    if (m_crtc == XCB_NONE) {
        return true;
    }
    ColorLUT lut(transformation, m_gammaRampSize);
    xcb_randr_set_crtc_gamma(kwinApp()->x11Connection(), m_crtc, lut.size(), lut.red(), lut.green(), lut.blue());
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

void X11Output::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

} // namespace KWin
