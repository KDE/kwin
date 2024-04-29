/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "core/drmdevice.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glutils.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/surface.h>

#include <cmath>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

namespace KWin
{
namespace Wayland
{

static const bool bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

WaylandEglPrimaryLayer::WaylandEglPrimaryLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : OutputLayer(output)
    , m_backend(backend)
{
}

WaylandEglPrimaryLayer::~WaylandEglPrimaryLayer()
{
}

GLFramebuffer *WaylandEglPrimaryLayer::fbo() const
{
    return m_buffer->framebuffer();
}

std::shared_ptr<GLTexture> WaylandEglPrimaryLayer::texture() const
{
    return m_buffer->texture();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglPrimaryLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        for (const uint32_t &candidateFormat : {DRM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB8888}) {
            auto it = formatTable.constFind(candidateFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), nativeSize, it.key(), it.value());
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    const QRegion repair = bufferAgeEnabled ? m_damageJournal.accumulate(m_buffer->age(), infiniteRegion()) : infiniteRegion();
    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = repair,
    };
}

bool WaylandEglPrimaryLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    // Flush rendering commands to the dmabuf.
    glFlush();
    EGLNativeFence releaseFence{m_backend->eglDisplayObject()};

    m_presentationBuffer = m_backend->backend()->importBuffer(m_buffer->buffer());
    m_swapchain->release(m_buffer, releaseFence.takeFileDescriptor());

    m_damageJournal.add(damagedRegion);
    return true;
}

bool WaylandEglPrimaryLayer::doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color)
{
    Q_ASSERT(!m_presentationBuffer);
    // TODO use viewporter to relax this check
    if (sourceRect() != targetRect() || targetRect() != QRectF(QPointF(0, 0), m_output->modeSize())) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal || color != ColorDescription::sRGB) {
        return false;
    }
    m_presentationBuffer = m_backend->backend()->importBuffer(buffer);
    return m_presentationBuffer;
}

void WaylandEglPrimaryLayer::present()
{
    const auto waylandOutput = static_cast<WaylandOutput *>(m_output);
    KWayland::Client::Surface *surface = waylandOutput->surface();
    if (m_presentationBuffer) {
        surface->attachBuffer(m_presentationBuffer);
        surface->damage(m_damageJournal.lastDamage());
        surface->setScale(std::ceil(waylandOutput->scale()));
        m_presentationBuffer = nullptr;
    }
    surface->commit();
}

DrmDevice *WaylandEglPrimaryLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandEglPrimaryLayer::supportedDrmFormats() const
{
    return m_backend->backend()->display()->linuxDmabuf()->formats();
}

WaylandEglCursorLayer::WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : OutputLayer(output)
    , m_backend(backend)
{
}

WaylandEglCursorLayer::~WaylandEglCursorLayer()
{
    m_backend->openglContext()->makeCurrent();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const auto tmp = targetRect().size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        uint32_t format = DRM_FORMAT_INVALID;
        QList<uint64_t> modifiers;
        for (const uint32_t &candidateFormat : {DRM_FORMAT_ARGB2101010, DRM_FORMAT_ARGB8888}) {
            auto it = formatTable.constFind(candidateFormat);
            if (it != formatTable.constEnd()) {
                format = it.key();
                modifiers = it.value();
                break;
            }
        }
        if (format == DRM_FORMAT_INVALID) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
        m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), bufferSize, format, modifiers);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandEglCursorLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_query));
    }
    // Flush rendering commands to the dmabuf.
    glFlush();

    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    static_cast<WaylandOutput *>(m_output)->cursor()->update(buffer, scale(), hotspot().toPoint());

    EGLNativeFence releaseFence{m_backend->eglDisplayObject()};
    m_swapchain->release(m_buffer, releaseFence.takeFileDescriptor());
    return true;
}

DrmDevice *WaylandEglCursorLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandEglCursorLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createEglWaylandOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *output) {
        m_outputs.erase(output);
    });

    b->setEglBackend(this);
}

WaylandEglBackend::~WaylandEglBackend()
{
    cleanup();
}

WaylandBackend *WaylandEglBackend::backend() const
{
    return m_backend;
}

DrmDevice *WaylandEglBackend::drmDevice() const
{
    return m_backend->drmDevice();
}

void WaylandEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool WaylandEglBackend::createEglWaylandOutput(Output *waylandOutput)
{
    m_outputs[waylandOutput] = Layers{
        .primaryLayer = std::make_unique<WaylandEglPrimaryLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
        .cursorLayer = std::make_unique<WaylandEglCursorLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
    };
    return true;
}

bool WaylandEglBackend::initializeEgl()
{
    initClientExtensions();

    if (!m_backend->sceneEglDisplayObject()) {
        for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
            if (!hasClientExtension(extension)) {
                qCWarning(KWIN_WAYLAND_BACKEND) << extension << "client extension is not supported by the platform";
                return false;
            }
        }

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->drmDevice()->gbmDevice(), nullptr)));
    }

    const auto display = m_backend->sceneEglDisplayObject();
    if (!display) {
        return false;
    }
    setEglDisplay(display);
    return true;
}

void WaylandEglBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initWayland();
}

bool WaylandEglBackend::initRenderingContext()
{
    if (!createContext(EGL_NO_CONFIG_KHR)) {
        return false;
    }

    auto waylandOutputs = m_backend->waylandOutputs();

    // we only allow to start with at least one output
    if (waylandOutputs.isEmpty()) {
        return false;
    }

    for (auto *out : waylandOutputs) {
        if (!createEglWaylandOutput(out)) {
            return false;
        }
    }

    if (m_outputs.empty()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Create Window Surfaces failed";
        return false;
    }

    return makeCurrent();
}

std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> WaylandEglBackend::textureForOutput(KWin::Output *output) const
{
    return std::make_pair(m_outputs.at(output).primaryLayer->texture(), ColorDescription::sRGB);
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

bool WaylandEglBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    m_outputs[output].primaryLayer->present();
    static_cast<WaylandOutput *>(output)->setPendingFrame(frame);
    Q_EMIT static_cast<WaylandOutput *>(output)->outputChange(frame->damage());
    return true;
}

OutputLayer *WaylandEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

OutputLayer *WaylandEglBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
}

#include "moc_wayland_egl_backend.cpp"
