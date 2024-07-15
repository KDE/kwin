/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_output.h"
#include "core/colorpipeline.h"
#include "core/colortransformation.h"
#include "main.h"
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

void X11Output::setWhitepoint(const QVector2D &whitePoint)
{
    if (m_crtc == XCB_NONE) {
        return;
    }
    const auto &sRGB = Colorimetry::fromName(NamedColorimetry::BT709);
    QVector3D channelFactors = sRGB.fromXYZ() * Colorimetry::xyToXYZ(whitePoint);
    // normalize to the biggest component, otherwise there can be values > 1
    channelFactors /= std::max(channelFactors.x(), std::max(channelFactors.y(), channelFactors.z()));

    ColorPipeline pipeline;
    pipeline.addTransferFunction(TransferFunction::gamma22, 1);
    pipeline.addMultiplier(channelFactors);
    pipeline.addInverseTransferFunction(TransferFunction::gamma22, 1);
    std::vector<uint16_t> red(m_gammaRampSize);
    std::vector<uint16_t> green(m_gammaRampSize);
    std::vector<uint16_t> blue(m_gammaRampSize);
    for (uint16_t i = 0; i < m_gammaRampSize; i++) {
        const double input = i / double(m_gammaRampSize - 1);
        const QVector3D output = pipeline.evaluate(QVector3D(input, input, input));
        red[i] = std::round(std::clamp(output.x(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max());
        red[i] = std::round(std::clamp(output.y(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max());
        green[i] = std::round(std::clamp(output.z(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max());
    }
    xcb_randr_set_crtc_gamma(kwinApp()->x11Connection(), m_crtc, m_gammaRampSize, red.data(), green.data(), blue.data());
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

#include "moc_x11_standalone_output.cpp"
