/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "opengl/glutils.h"
#include "wayland_backend.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include <cmath>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

namespace KWin
{
namespace Wayland
{

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b, RenderDevice *device)
    : EglBackend(device)
    , m_backend(b)
{
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createOutputLayers);

    b->setEglBackend(this);
}

WaylandEglBackend::~WaylandEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<WaylandOutput *>(output)->setOutputLayers({});
    }
    cleanup();
}

WaylandBackend *WaylandEglBackend::backend() const
{
    return m_backend;
}

void WaylandEglBackend::createOutputLayers(BackendOutput *output)
{
    const auto waylandOutput = static_cast<WaylandOutput *>(output);
    std::vector<std::unique_ptr<OutputLayer>> layers;
    auto primary = std::make_unique<WaylandLayer>(waylandOutput, this, backend()->display(), OutputLayerType::Primary, 0);
    primary->subSurface()->placeAbove(waylandOutput->surface());
    layers.push_back(std::move(primary));
    for (int z = 1; z < 5; z++) {
        auto layer = std::make_unique<WaylandLayer>(waylandOutput, this, backend()->display(), OutputLayerType::GenericLayer, z);
        layer->subSurface()->placeAbove(static_cast<WaylandLayer *>(layers.back().get())->surface());
        layers.push_back(std::move(layer));
    }
    layers.push_back(std::make_unique<WaylandCursorLayer>(waylandOutput, this, backend()->display()));
    waylandOutput->setOutputLayers(std::move(layers));
}

bool WaylandEglBackend::init()
{
    if (!initClientExtensions()) {
        qCWarning(KWIN_WAYLAND_BACKEND, "Could not initialize egl");
        return false;
    }
    if (!initRenderingContext()) {
        qCWarning(KWIN_WAYLAND_BACKEND, "Could not initialize rendering context");
        return false;
    }
    initWayland();
    return true;
}

bool WaylandEglBackend::initRenderingContext()
{
    if (!createContext()) {
        return false;
    }

    auto waylandOutputs = m_backend->waylandOutputs();

    // we only allow to start with at least one output
    if (waylandOutputs.isEmpty()) {
        return false;
    }

    for (auto *out : waylandOutputs) {
        createOutputLayers(out);
    }

    return openglContext()->makeCurrent();
}

QList<OutputLayer *> WaylandEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<WaylandOutput *>(output)->outputLayers();
}

}
}

#include "moc_wayland_egl_backend.cpp"
