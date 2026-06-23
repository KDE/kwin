/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_wayland.h"
#include "core/backendoutput.h"
#include "core/drmdevice.h"
#include "core/renderbackend.h"
#include "texture.h"
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

    connect(surface, &SurfaceInterface::mapped,
            this, &SurfaceItemWayland::handleSurfaceMappedChanged);
    connect(surface, &SurfaceInterface::unmapped,
            this, &SurfaceItemWayland::handleSurfaceMappedChanged);
    setVisible(surface->isMapped());

    SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(subsurface, &SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
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
}

RegionF SurfaceItemWayland::shape() const
{
    return RegionF{rect()};
}

RegionF SurfaceItemWayland::opaque() const
{
    if (m_surface) {
        return m_surface->opaque();
    }
    return RegionF();
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

void SurfaceItemWayland::handleSurfaceMappedChanged()
{
    setVisible(m_surface->isMapped());
}

ContentType SurfaceItemWayland::contentType() const
{
    return m_surface ? m_surface->contentType() : ContentType::None;
}

void SurfaceItemWayland::setScanoutHint(DrmDevice *device, const FormatModifierMap &drmFormats)
{
    if (!m_surface || !m_surface->dmabufFeedbackV1()) {
        return;
    }
    if (!device && m_scanoutFeedback.has_value()) {
        m_surface->dmabufFeedbackV1()->setScanoutTranches({});
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
}

void SurfaceItemWayland::handleColorDescriptionChanged()
{
    auto description = m_surface->colorDescription();
    if (m_surface->colorDescriptionType() == ColorDescriptionType::Windows) {
        // TODO also react to config changes after the image description is set?
        const auto group = kwinApp()->config()->group("Windows_HDR");
        const double reference = group.readEntry("Reference", 203.0);
        description = description->withReference(reference);
        // the max average is intentionally ignored, since the calibration app doesn't set it (anymore)
        description = description->withHdrMetadata(reference, group.readEntry("MaxLuminance", 1'000));
    }
    setColorDescription(description);
    setRenderingIntent(m_surface->renderingIntent());
}

void SurfaceItemWayland::handlePresentationModeHintChanged()
{
    setPresentationHint(m_surface->presentationModeHint());
}

void SurfaceItemWayland::handleReleasePointChanged()
{
    m_bufferReleasePoint = m_surface->bufferReleasePoint();
    if (m_texture) {
        m_texture->setReleasePoint(m_bufferReleasePoint);
    }
}

void SurfaceItemWayland::handleAlphaMultiplierChanged()
{
    setOpacity(m_surface->alphaMultiplier());
}

void SurfaceItemWayland::handleFramePainted(LogicalOutput *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
    if (!m_surface) {
        return;
    }
    m_surface->frameRendered(timestamp.count());
    if (frame) {
        // FIXME make frame always valid
        if (auto feedback = m_surface->presentationFeedback(output)) {
            frame->addFeedback(std::move(feedback));
        }
    }
    // TODO only call this once per refresh cycle
    m_surface->clearFifoBarrier(output ? std::optional(std::chrono::nanoseconds(1'000'000'000'000) / output->refreshRate()) : std::nullopt);
}

#if KWIN_BUILD_X11
SurfaceItemXwayland::SurfaceItemXwayland(X11Window *window, Item *parent)
    : SurfaceItemWayland(window->surface(), parent)
    , m_window(window)
{
    connect(window, &X11Window::shapeChanged, this, &SurfaceItemXwayland::handleShapeChange);
}

void SurfaceItemXwayland::handleShapeChange()
{
    const auto newShape = m_window->shapeRegion();
    scheduleRepaint(newShape.xored(m_previousBufferShape));
    m_previousBufferShape = newShape;
    discardQuads();
}

RegionF SurfaceItemXwayland::shape() const
{
    return m_window->shapeRegion() & rect();
}

RegionF SurfaceItemXwayland::opaque() const
{
    if (!m_window->hasAlpha()) {
        return shape();
    } else {
        return m_window->opaqueRegion() & shape();
    }
    return RegionF();
}
#endif

} // namespace KWin

#include "moc_surfaceitem_wayland.cpp"
