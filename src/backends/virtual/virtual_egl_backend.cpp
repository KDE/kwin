/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
#include "utils/vsyncsource.h"
#include "virtual_backend.h"
#include "virtual_layer.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b, RenderDevice *renderDevice)
    : EglBackend(renderDevice)
    , m_backend(b)
{
}

VirtualEglBackend::~VirtualEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<VirtualOutput *>(output)->setOutputLayer(nullptr);
    }
    cleanup();
}

VirtualBackend *VirtualEglBackend::backend() const
{
    return m_backend;
}

bool VirtualEglBackend::init()
{
    if (!initClientExtensions()) {
        qCWarning(KWIN_VIRTUAL, "Could not initialize egl");
        return false;
    }
    if (!createContext()) {
        qCWarning(KWIN_VIRTUAL, "Could not initialize rendering context");
        return false;
    }
    initWayland();

    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputAdded, this, &VirtualEglBackend::addOutput);
    return true;
}

void VirtualEglBackend::addOutput(BackendOutput *output)
{
    static_cast<VirtualOutput *>(output)->setOutputLayer(std::make_unique<VirtualLayer>(output, this));
}

QList<OutputLayer *> VirtualEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return {static_cast<VirtualOutput *>(output)->outputLayer()};
}

} // namespace

#include "moc_virtual_egl_backend.cpp"
