/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
#include "core/drmdevice.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "core/renderdevice.h"
#include "opengl/glutils.h"
#include "swapchain.h"
#include "utils/vsyncsource.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualEglLayer::VirtualEglLayer(BackendOutput *output, VirtualEglBackend *backend)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_backend(backend)
{
}

VirtualEglLayer::~VirtualEglLayer()
{
    (void)m_backend->openglContext()->makeCurrent();
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::beginFrame(OutputFrame *frame)
{
    if (!m_backend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        GraphicsBufferOptions options{
            .size = nativeSize,
            .format = DRM_FORMAT_XRGB8888,
            .modifiers = m_backend->supportedFormats()[DRM_FORMAT_XRGB8888],
            .software = false,
            .scanout = false,
        };
        m_swapchain = Swapchain::create(m_backend->renderDevice()->allocator(), options);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    auto fbo = m_backend->openglContext()->importDmaBufAsFbo(m_current->buffer());
    if (!fbo) {
        return std::nullopt;
    }

    fbo->colorAttachment()->setContentTransform(bufferTransform().combine(OutputTransform::FlipY));

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(fbo),
        .repaint = m_damageJournal.accumulate(m_current->age(), Region::infinite()),
    };
}

bool VirtualEglLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    m_swapchain->release(m_current.get(), std::move(sync));
    m_damageJournal.add(damagedDeviceRegion);
    return true;
}

FormatModifierMap VirtualEglLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void VirtualEglLayer::releaseBuffers()
{
    m_current.reset();
    m_swapchain.reset();
}

GLTexture *VirtualEglLayer::texture() const
{
    if (!m_current) {
        return nullptr;
    }
    return m_backend->openglContext()->importDmaBufAsTexture(m_current->buffer());
}

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
    static_cast<VirtualOutput *>(output)->setOutputLayer(std::make_unique<VirtualEglLayer>(output, this));
}

QList<OutputLayer *> VirtualEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return {static_cast<VirtualOutput *>(output)->outputLayer()};
}

} // namespace

#include "moc_virtual_egl_backend.cpp"
