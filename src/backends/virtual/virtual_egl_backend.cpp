/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
#include "core/drmdevice.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glutils.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

static const bool s_bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

VirtualEglLayer::VirtualEglLayer(Output *output, VirtualEglBackend *backend)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_backend(backend)
{
}

VirtualEglLayer::~VirtualEglLayer()
{
    m_backend->openglContext()->makeCurrent();
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::doBeginFrame()
{
    m_backend->openglContext()->makeCurrent();

    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), nativeSize, DRM_FORMAT_XRGB8888, m_backend->supportedFormats()[DRM_FORMAT_XRGB8888]);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->framebuffer()),
        .repaint = s_bufferAgeEnabled ? m_damageJournal.accumulate(m_current->age(), infiniteRegion()) : infiniteRegion(),
    };
}

bool VirtualEglLayer::doEndFrame(const QRegion &renderedDeviceRegion, const QRegion &damagedDeviceRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    glFlush(); // flush pending rendering commands.
    m_swapchain->release(m_current, FileDescriptor{});
    m_damageJournal.add(damagedDeviceRegion);
    return true;
}

DrmDevice *VirtualEglLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> VirtualEglLayer::supportedDrmFormats() const
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
    return m_current ? m_current->texture().get() : nullptr;
}

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b)
    : m_backend(b)
{
}

VirtualEglBackend::~VirtualEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        static_cast<VirtualOutput *>(output)->setOutputLayer(nullptr);
    }
    cleanup();
}

VirtualBackend *VirtualEglBackend::backend() const
{
    return m_backend;
}

DrmDevice *VirtualEglBackend::drmDevice() const
{
    return m_backend->drmDevice();
}

bool VirtualEglBackend::initializeEgl()
{
    initClientExtensions();

    if (!m_backend->sceneEglDisplayObject()) {
        for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
            if (!hasClientExtension(extension)) {
                qCWarning(KWIN_VIRTUAL) << extension << "client extension is not supported by the platform";
                return false;
            }
        }

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->drmDevice()->gbmDevice(), nullptr)));
    }

    auto display = m_backend->sceneEglDisplayObject();
    if (!display) {
        return false;
    }
    setEglDisplay(display);
    return true;
}

void VirtualEglBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    initWayland();

    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputAdded, this, &VirtualEglBackend::addOutput);
}

bool VirtualEglBackend::initRenderingContext()
{
    return createContext(EGL_NO_CONFIG_KHR) && openglContext()->makeCurrent();
}

void VirtualEglBackend::addOutput(Output *output)
{
    openglContext()->makeCurrent();
    static_cast<VirtualOutput *>(output)->setOutputLayer(std::make_unique<VirtualEglLayer>(output, this));
}

QList<OutputLayer *> VirtualEglBackend::compatibleOutputLayers(Output *output)
{
    return {static_cast<VirtualOutput *>(output)->outputLayer()};
}

} // namespace

#include "moc_virtual_egl_backend.cpp"
