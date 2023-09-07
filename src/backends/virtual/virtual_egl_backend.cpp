/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "libkwineffects/glutils.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "platformsupport/scenes/opengl/eglswapchain.h"
#include "platformsupport/scenes/opengl/glrendertimequery.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualEglLayer::VirtualEglLayer(Output *output, VirtualEglBackend *backend)
    : m_backend(backend)
    , m_output(output)
{
}

std::shared_ptr<GLTexture> VirtualEglLayer::texture() const
{
    return m_current->texture();
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::beginFrame()
{
    m_backend->makeCurrent();

    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = EglSwapchain::create(m_backend->graphicsBufferAllocator(), m_backend->contextObject(), nativeSize, DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_INVALID});
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool VirtualEglLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_query->end();
    glFlush(); // flush pending rendering commands.
    Q_EMIT m_output->outputChange(damagedRegion);
    return true;
}

quint32 VirtualEglLayer::format() const
{
    return DRM_FORMAT_XRGB8888;
}

std::chrono::nanoseconds VirtualEglLayer::queryRenderTime() const
{
    if (m_query) {
        m_backend->makeCurrent();
        return m_query->result();
    } else {
        return std::chrono::nanoseconds::zero();
    }
}

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
    , m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(b->gbmDevice()))
{
}

VirtualEglBackend::~VirtualEglBackend()
{
    m_outputs.clear();
    cleanup();
}

VirtualBackend *VirtualEglBackend::backend() const
{
    return m_backend;
}

GraphicsBufferAllocator *VirtualEglBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
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

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->gbmDevice(), nullptr)));
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

    initKWinGL();
    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    setSupportsBufferAge(false);
    initWayland();

    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputAdded, this, &VirtualEglBackend::addOutput);
    connect(m_backend, &VirtualBackend::outputRemoved, this, &VirtualEglBackend::removeOutput);
}

bool VirtualEglBackend::initRenderingContext()
{
    return createContext(EGL_NO_CONFIG_KHR) && makeCurrent();
}

void VirtualEglBackend::addOutput(Output *output)
{
    makeCurrent();
    m_outputs[output] = std::make_unique<VirtualEglLayer>(output, this);
}

void VirtualEglBackend::removeOutput(Output *output)
{
    makeCurrent();
    m_outputs.erase(output);
}

std::unique_ptr<SurfaceTexture> VirtualEglBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

OutputLayer *VirtualEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

void VirtualEglBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> VirtualEglBackend::textureForOutput(Output *output) const
{
    auto it = m_outputs.find(output);
    if (it == m_outputs.end()) {
        return {nullptr, ColorDescription::sRGB};
    }
    return std::make_pair(it->second->texture(), ColorDescription::sRGB);
}

} // namespace

#include "moc_virtual_egl_backend.cpp"
