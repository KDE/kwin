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
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualEglLayer::VirtualEglLayer(Output *output, VirtualEglBackend *backend)
    : OutputLayer(output)
    , m_backend(backend)
{
}

std::shared_ptr<GLTexture> VirtualEglLayer::texture() const
{
    return m_current->texture();
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::doBeginFrame()
{
    m_backend->makeCurrent();

    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), nativeSize, DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_INVALID});
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
        .repaint = infiniteRegion(),
    };
}

bool VirtualEglLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    glFlush(); // flush pending rendering commands.
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

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
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

bool VirtualEglBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    static_cast<VirtualOutput *>(output)->present(frame);
    return true;
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
