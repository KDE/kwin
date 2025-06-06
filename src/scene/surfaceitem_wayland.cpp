/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_wayland.h"
#include "core/drmdevice.h"
#include "core/renderbackend.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/subcompositor.h"
#include "wayland/surface.h"
#include "window.h"

#if KWIN_BUILD_X11
#include "x11window.h"
#endif

namespace KWin
{

SurfaceItemWayland::SurfaceItemWayland(SurfaceInterface *surface, Item *parent)
    : SurfaceItem(parent)
    , m_surface(surface)
{
    connect(surface, &SurfaceInterface::sizeChanged,
            this, &SurfaceItemWayland::handleSurfaceSizeChanged);
    connect(surface, &SurfaceInterface::bufferChanged,
            this, &SurfaceItemWayland::handleBufferChanged);
    connect(surface, &SurfaceInterface::bufferSourceBoxChanged,
            this, &SurfaceItemWayland::handleBufferSourceBoxChanged);
    connect(surface, &SurfaceInterface::bufferTransformChanged,
            this, &SurfaceItemWayland::handleBufferTransformChanged);

    connect(surface, &SurfaceInterface::childSubSurfacesChanged,
            this, &SurfaceItemWayland::handleChildSubSurfacesChanged);
    connect(surface, &SurfaceInterface::committed,
            this, &SurfaceItemWayland::handleSurfaceCommitted);
    connect(surface, &SurfaceInterface::damaged,
            this, &SurfaceItemWayland::addDamage);
    connect(surface, &SurfaceInterface::childSubSurfaceRemoved,
            this, &SurfaceItemWayland::handleChildSubSurfaceRemoved);
    connect(surface, &SurfaceInterface::colorDescriptionChanged,
            this, &SurfaceItemWayland::handleColorDescriptionChanged);
    connect(surface, &SurfaceInterface::presentationModeHintChanged,
            this, &SurfaceItemWayland::handlePresentationModeHintChanged);
    connect(surface, &SurfaceInterface::bufferReleasePointChanged, this, &SurfaceItemWayland::handleReleasePointChanged);
    connect(surface, &SurfaceInterface::alphaMultiplierChanged, this, &SurfaceItemWayland::handleAlphaMultiplierChanged);

    SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(surface, &SurfaceInterface::mapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(surface, &SurfaceInterface::unmapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(subsurface, &SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
        setVisible(surface->isMapped());
        setPosition(subsurface->position());
    }

    handleChildSubSurfacesChanged();
    setDestinationSize(surface->size());
    setBufferTransform(surface->bufferTransform());
    setBufferSourceBox(surface->bufferSourceBox());
    setBuffer(surface->buffer());
    m_bufferReleasePoint = m_surface->bufferReleasePoint();
    setColorDescription(surface->colorDescription());
    setRenderingIntent(surface->renderingIntent());
    setPresentationHint(surface->presentationModeHint());
    setOpacity(surface->alphaMultiplier());

    m_fifoFallbackTimer.setInterval(1000 / 20);
    m_fifoFallbackTimer.setSingleShot(true);
    connect(&m_fifoFallbackTimer, &QTimer::timeout, this, &SurfaceItemWayland::handleFifoFallback);
}

QList<QRectF> SurfaceItemWayland::shape() const
{
    return {rect()};
}

QRegion SurfaceItemWayland::opaque() const
{
    if (m_surface) {
        return m_surface->opaque();
    }
    return QRegion();
}

SurfaceInterface *SurfaceItemWayland::surface() const
{
    return m_surface;
}

void SurfaceItemWayland::handleSurfaceSizeChanged()
{
    setDestinationSize(m_surface->size());
}

void SurfaceItemWayland::handleBufferChanged()
{
    setBuffer(m_surface->buffer());
}

void SurfaceItemWayland::handleBufferSourceBoxChanged()
{
    setBufferSourceBox(m_surface->bufferSourceBox());
}

void SurfaceItemWayland::handleBufferTransformChanged()
{
    setBufferTransform(m_surface->bufferTransform());
}

void SurfaceItemWayland::handleSurfaceCommitted()
{
    if (m_surface->hasFifoBarrier()) {
        m_fifoFallbackTimer.start();
    }
    if (m_surface->hasFrameCallbacks() || m_surface->hasFifoBarrier() || m_surface->hasPresentationFeedback()) {
        scheduleFrame();
    }
}

SurfaceItemWayland *SurfaceItemWayland::getOrCreateSubSurfaceItem(SubSurfaceInterface *child)
{
    auto &item = m_subsurfaces[child];
    if (!item) {
        item = std::make_unique<SurfaceItemWayland>(child->surface(), this);
    }
    return item.get();
}

void SurfaceItemWayland::handleChildSubSurfaceRemoved(SubSurfaceInterface *child)
{
    m_subsurfaces.erase(child);
}

void SurfaceItemWayland::handleChildSubSurfacesChanged()
{
    const QList<SubSurfaceInterface *> below = m_surface->below();
    const QList<SubSurfaceInterface *> above = m_surface->above();

    for (int i = 0; i < below.count(); ++i) {
        SurfaceItemWayland *subsurfaceItem = getOrCreateSubSurfaceItem(below[i]);
        subsurfaceItem->setZ(i - below.count());
    }

    for (int i = 0; i < above.count(); ++i) {
        SurfaceItemWayland *subsurfaceItem = getOrCreateSubSurfaceItem(above[i]);
        subsurfaceItem->setZ(i);
    }
}

void SurfaceItemWayland::handleSubSurfacePositionChanged()
{
    setPosition(m_surface->subSurface()->position());
}

void SurfaceItemWayland::handleSubSurfaceMappedChanged()
{
    setVisible(m_surface->isMapped());
}

ContentType SurfaceItemWayland::contentType() const
{
    return m_surface ? m_surface->contentType() : ContentType::None;
}

void SurfaceItemWayland::setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats)
{
    if (!m_surface || !m_surface->dmabufFeedbackV1()) {
        return;
    }
    if (!device && m_scanoutFeedback.has_value()) {
        m_surface->dmabufFeedbackV1()->setTranches({});
        m_scanoutFeedback.reset();
        return;
    }
    if (!m_scanoutFeedback || m_scanoutFeedback->device != device || m_scanoutFeedback->formats != drmFormats) {
        m_scanoutFeedback = ScanoutFeedback{
            .device = device,
            .formats = drmFormats,
        };
        m_surface->dmabufFeedbackV1()->setScanoutTranches(device, drmFormats);
    }
}

void SurfaceItemWayland::freeze()
{
    if (!m_surface) {
        return;
    }

    m_surface->disconnect(this);
    if (auto subsurface = m_surface->subSurface()) {
        subsurface->disconnect(this);
    }

    for (auto &[subsurface, subsurfaceItem] : m_subsurfaces) {
        subsurfaceItem->freeze();
    }

    m_surface = nullptr;
    m_fifoFallbackTimer.stop();
}

void SurfaceItemWayland::handleColorDescriptionChanged()
{
    setColorDescription(m_surface->colorDescription());
    setRenderingIntent(m_surface->renderingIntent());
}

void SurfaceItemWayland::handlePresentationModeHintChanged()
{
    setPresentationHint(m_surface->presentationModeHint());
}

void SurfaceItemWayland::handleReleasePointChanged()
{
    m_bufferReleasePoint = m_surface->bufferReleasePoint();
}

void SurfaceItemWayland::handleAlphaMultiplierChanged()
{
    setOpacity(m_surface->alphaMultiplier());
}

void SurfaceItemWayland::handleFramePainted(Output *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
    if (!m_surface) {
        return;
    }
    m_surface->frameRendered(timestamp.count());
    if (frame) {
        // FIXME make frame always valid
        if (auto feedback = m_surface->takePresentationFeedback(output)) {
            frame->addFeedback(std::move(feedback));
        }
    }
    // TODO only call this once per refresh cycle
    m_surface->clearFifoBarrier();
    if (m_fifoFallbackTimer.isActive() && output) {
        // TODO once we can rely on frame being not-nullptr, use its refresh duration instead
        const auto refreshDuration = std::chrono::nanoseconds(1'000'000'000'000) / output->refreshRate();
        // some games don't work properly if the refresh rate goes too low with FIFO. 30Hz is assumed to be fine here.
        // this must still be slower than the actual screen though, or fifo behavior would be broken!
        const auto fallbackRefreshDuration = std::max(refreshDuration * 5 / 4, std::chrono::nanoseconds(1'000'000'000) / 30);
        // reset the timer, it should only trigger if we don't present fast enough
        m_fifoFallbackTimer.start(std::chrono::duration_cast<std::chrono::milliseconds>(fallbackRefreshDuration));
    }
}

void SurfaceItemWayland::handleFifoFallback()
{
    if (m_surface) {
        m_surface->clearFifoBarrier();
    }
}

#if KWIN_BUILD_X11
SurfaceItemXwayland::SurfaceItemXwayland(X11Window *window, Item *parent)
    : SurfaceItemWayland(window->surface(), parent)
    , m_window(window)
{
    connect(window, &X11Window::shapeChanged, this, &SurfaceItemXwayland::discardQuads);
}

QList<QRectF> SurfaceItemXwayland::shape() const
{
    QList<QRectF> shape = m_window->shapeRegion();
    for (QRectF &shapePart : shape) {
        shapePart = shapePart.intersected(rect());
    }
    return shape;
}

QRegion SurfaceItemXwayland::opaque() const
{
    QRegion shapeRegion;
    for (const QRectF &shapePart : shape()) {
        shapeRegion += shapePart.toRect();
    }
    if (!m_window->hasAlpha()) {
        return shapeRegion;
    } else {
        return m_window->opaqueRegion() & shapeRegion;
    }
    return QRegion();
}
#endif
} // namespace KWin

#include "moc_surfaceitem_wayland.cpp"
