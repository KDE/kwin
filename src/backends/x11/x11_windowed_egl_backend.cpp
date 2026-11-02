/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_layers.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <drm_fourcc.h>

namespace KWin
{

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend, RenderDevice *device)
    : EglBackend(device)
    , m_backend(backend)
{
}

X11WindowedEglBackend::~X11WindowedEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<X11WindowedOutput *>(output)->setOutputLayers({});
    }
    cleanup();
}

X11WindowedBackend *X11WindowedEglBackend::backend() const
{
    return m_backend;
}

bool X11WindowedEglBackend::init()
{
    qputenv("EGL_PLATFORM", "x11");

    if (!initClientExtensions()) {
        qCWarning(KWIN_X11WINDOWED, "Could not initialize egl");
        return false;
    }
    if (!createContext()) {
        qCWarning(KWIN_X11WINDOWED, "Could not initialize rendering context");
        return false;
    }

    initWayland();

    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
        std::vector<std::unique_ptr<OutputLayer>> layers;
        layers.push_back(std::make_unique<X11WindowedPrimaryLayer>(backend(), this, x11Output));
        layers.push_back(std::make_unique<X11WindowedCursorLayer>(backend(), this, x11Output));
        x11Output->setOutputLayers(std::move(layers));
    }
    return true;
}

QList<OutputLayer *> X11WindowedEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<X11WindowedOutput *>(output)->outputLayers();
}

} // namespace

#include "moc_x11_windowed_egl_backend.cpp"
