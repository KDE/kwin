/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "surface.h"
#include "blur.h"
#include "clientconnection.h"
#include "colormanagement_v1.h"
#include "compositor.h"
#include "contrast.h"
#include "display.h"
#include "fractionalscale_v1_p.h"
#include "frog_colormanagement_v1.h"
#include "idleinhibit_v1_p.h"
#include "linux_drm_syncobj_v1.h"
#include "linuxdmabufv1clientbuffer.h"
#include "output.h"
#include "pointerconstraints_v1_p.h"
#include "presentationtime.h"
#include "region_p.h"
#include "shadow.h"
#include "slide.h"
#include "subcompositor.h"
#include "surface_p.h"
#include "transaction.h"
#include "utils/resource.h"

#include <wayland-server.h>
// std
#include <algorithm>
#include <cmath>

namespace KWin
{

static QRegion map_helper(const QMatrix4x4 &matrix, const QRegion &region)
{
    QRegion result;
    for (const QRect &rect : region) {
        result += matrix.mapRect(QRectF(rect)).toAlignedRect();
    }
    return result;
}

SurfaceRole::SurfaceRole(const QByteArray &name)
    : m_name(name)
{
}

QByteArray SurfaceRole::name() const
{
    return m_name;
}

SurfaceInterfacePrivate::SurfaceInterfacePrivate(SurfaceInterface *q)
    : q(q)
    , current(std::make_unique<SurfaceState>())
    , pending(std::make_unique<SurfaceState>())
{
}

void SurfaceInterfacePrivate::addChild(SubSurfaceInterface *child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    current->subsurface.above.append(child);
    pending->subsurface.above.append(child);

    if (subsurface.transaction) {
        subsurface.transaction->amend(q, [child](SurfaceState *state) {
            state->subsurface.above.append(child);
        });
    }

    for (auto transaction = firstTransaction; transaction; transaction = transaction->next(q)) {
        transaction->amend(q, [child](SurfaceState *state) {
            state->subsurface.above.append(child);
        });
    }

    child->surface()->setOutputs(outputs, primaryOutput);
    if (preferredBufferScale.has_value()) {
        child->surface()->setPreferredBufferScale(preferredBufferScale.value());
    }
    if (preferredBufferTransform.has_value()) {
        child->surface()->setPreferredBufferTransform(preferredBufferTransform.value());
    }
    if (preferredColorDescription) {
        child->surface()->setPreferredColorDescription(preferredColorDescription.value());
    }

    Q_EMIT q->childSubSurfaceAdded(child);
    Q_EMIT q->childSubSurfacesChanged();
}

void SurfaceInterfacePrivate::removeChild(SubSurfaceInterface *child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    current->subsurface.below.removeAll(child);
    current->subsurface.above.removeAll(child);

    pending->subsurface.below.removeAll(child);
    pending->subsurface.above.removeAll(child);

    if (subsurface.transaction) {
        subsurface.transaction->amend(q, [child](SurfaceState *state) {
            state->subsurface.below.removeOne(child);
            state->subsurface.above.removeOne(child);
        });
    }

    for (auto transaction = firstTransaction; transaction; transaction = transaction->next(q)) {
        transaction->amend(q, [child](SurfaceState *state) {
            state->subsurface.below.removeOne(child);
            state->subsurface.above.removeOne(child);
        });
    }

    Q_EMIT q->childSubSurfaceRemoved(child);
    Q_EMIT q->childSubSurfacesChanged();
}

bool SurfaceInterfacePrivate::raiseChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor)
{
    Q_ASSERT(subsurface->parentSurface() == q);

    QList<SubSurfaceInterface *> *anchorList;
    int anchorIndex;

    pending->subsurface.below.removeOne(subsurface);
    pending->subsurface.above.removeOne(subsurface);

    if (anchor == q) {
        // Pretend as if the parent surface were before the first child in the above list.
        anchorList = &pending->subsurface.above;
        anchorIndex = -1;
    } else if (anchorIndex = pending->subsurface.above.indexOf(anchor->subSurface()); anchorIndex != -1) {
        anchorList = &pending->subsurface.above;
    } else if (anchorIndex = pending->subsurface.below.indexOf(anchor->subSurface()); anchorIndex != -1) {
        anchorList = &pending->subsurface.below;
    } else {
        return false; // The anchor belongs to other sub-surface tree.
    }

    anchorList->insert(anchorIndex + 1, subsurface);
    pending->subsurfaceOrderChanged = true;
    return true;
}

bool SurfaceInterfacePrivate::lowerChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor)
{
    Q_ASSERT(subsurface->parentSurface() == q);

    QList<SubSurfaceInterface *> *anchorList;
    int anchorIndex;

    pending->subsurface.below.removeOne(subsurface);
    pending->subsurface.above.removeOne(subsurface);

    if (anchor == q) {
        // Pretend as if the parent surface were after the last child in the below list.
        anchorList = &pending->subsurface.below;
        anchorIndex = pending->subsurface.below.count();
    } else if (anchorIndex = pending->subsurface.above.indexOf(anchor->subSurface()); anchorIndex != -1) {
        anchorList = &pending->subsurface.above;
    } else if (anchorIndex = pending->subsurface.below.indexOf(anchor->subSurface()); anchorIndex != -1) {
        anchorList = &pending->subsurface.below;
    } else {
        return false; // The anchor belongs to other sub-surface tree.
    }

    anchorList->insert(anchorIndex, subsurface);
    pending->subsurfaceOrderChanged = true;
    return true;
}

void SurfaceInterfacePrivate::setShadow(const QPointer<ShadowInterface> &shadow)
{
    pending->shadow = shadow;
    pending->shadowIsSet = true;
}

void SurfaceInterfacePrivate::setBlur(const QPointer<BlurInterface> &blur)
{
    pending->blur = blur;
    pending->blurIsSet = true;
}

void SurfaceInterfacePrivate::setSlide(const QPointer<SlideInterface> &slide)
{
    pending->slide = slide;
    pending->slideIsSet = true;
}

void SurfaceInterfacePrivate::setContrast(const QPointer<ContrastInterface> &contrast)
{
    pending->contrast = contrast;
    pending->contrastIsSet = true;
}

void SurfaceInterfacePrivate::installPointerConstraint(LockedPointerV1Interface *lock)
{
    Q_ASSERT(!lockedPointer);
    Q_ASSERT(!confinedPointer);

    lockedPointer = lock;

    auto cleanUp = [this]() {
        lockedPointer = nullptr;
        QObject::disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        QObject::disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT q->pointerConstraintsChanged();
    };

    if (lock->lifeTime() == LockedPointerV1Interface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(lock, &LockedPointerV1Interface::lockedChanged, q, [this, cleanUp] {
            if (lockedPointer->isLocked()) {
                return;
            }
            cleanUp();
        });
    }
    constrainsUnboundConnection = QObject::connect(lock, &LockedPointerV1Interface::destroyed, q, cleanUp);
    Q_EMIT q->pointerConstraintsChanged();
}

void SurfaceInterfacePrivate::installPointerConstraint(ConfinedPointerV1Interface *confinement)
{
    Q_ASSERT(!lockedPointer);
    Q_ASSERT(!confinedPointer);

    confinedPointer = confinement;

    auto cleanUp = [this]() {
        confinedPointer = nullptr;
        QObject::disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        QObject::disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        Q_EMIT q->pointerConstraintsChanged();
    };

    if (confinement->lifeTime() == ConfinedPointerV1Interface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(confinement, &ConfinedPointerV1Interface::confinedChanged, q, [this, cleanUp] {
            if (confinedPointer->isConfined()) {
                return;
            }
            cleanUp();
        });
    }
    constrainsUnboundConnection = QObject::connect(confinement, &ConfinedPointerV1Interface::destroyed, q, cleanUp);
    Q_EMIT q->pointerConstraintsChanged();
}

void SurfaceInterfacePrivate::installIdleInhibitor(IdleInhibitorV1Interface *inhibitor)
{
    idleInhibitors << inhibitor;
    if (idleInhibitors.count() == 1) {
        Q_EMIT q->inhibitsIdleChanged();
    }
}

void SurfaceInterfacePrivate::removeIdleInhibitor(IdleInhibitorV1Interface *inhibitor)
{
    idleInhibitors.removeOne(inhibitor);
    if (idleInhibitors.isEmpty()) {
        Q_EMIT q->inhibitsIdleChanged();
    }
}

void SurfaceInterfacePrivate::surface_destroy_resource(Resource *)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void SurfaceInterfacePrivate::surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SurfaceInterfacePrivate::surface_attach(Resource *resource, struct ::wl_resource *buffer, int32_t x, int32_t y)
{
    if (wl_resource_get_version(resource->handle) >= WL_SURFACE_OFFSET_SINCE_VERSION) {
        if (x != 0 || y != 0) {
            wl_resource_post_error(resource->handle, error_invalid_offset, "wl_surface.attach offset must be 0");
            return;
        }
    } else {
        pending->offset = QPoint(x, y);
    }

    pending->bufferIsSet = true;
    if (!buffer) {
        // got a null buffer, deletes content in next frame
        pending->buffer = nullptr;
        pending->damage = QRegion();
        pending->bufferDamage = QRegion();
        return;
    }
    pending->buffer = Display::bufferForResource(buffer);
}

void SurfaceInterfacePrivate::surface_damage(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    pending->damage += QRect(x, y, width, height);
}

void SurfaceInterfacePrivate::surface_frame(Resource *resource, uint32_t callback)
{
    wl_resource *callbackResource = wl_resource_create(resource->client(),
                                                       &wl_callback_interface,
                                                       /* version */ 1,
                                                       callback);
    if (!callbackResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    wl_resource_set_implementation(callbackResource, nullptr, nullptr, [](wl_resource *resource) {
        wl_list_remove(wl_resource_get_link(resource));
    });

    wl_list_insert(pending->frameCallbacks.prev, wl_resource_get_link(callbackResource));
}

void SurfaceInterfacePrivate::surface_set_opaque_region(Resource *resource, struct ::wl_resource *region)
{
    RegionInterface *r = RegionInterface::get(region);
    pending->opaque = r ? r->region() : QRegion();
    pending->opaqueIsSet = true;
}

void SurfaceInterfacePrivate::surface_set_input_region(Resource *resource, struct ::wl_resource *region)
{
    RegionInterface *r = RegionInterface::get(region);
    pending->input = r ? r->region() : infiniteRegion();
    pending->inputIsSet = true;
}

void SurfaceInterfacePrivate::surface_commit(Resource *resource)
{
    const bool sync = subsurface.handle && subsurface.handle->isSynchronized();

    if (syncObjV1 && syncObjV1->maybeEmitProtocolErrors()) {
        return;
    }

    Transaction *transaction;
    if (sync) {
        if (!subsurface.transaction) {
            subsurface.transaction = std::make_unique<Transaction>();
        }
        transaction = subsurface.transaction.get();
    } else {
        transaction = new Transaction();
    }

    for (SubSurfaceInterface *subsurface : std::as_const(pending->subsurface.below)) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(subsurface->surface());
        if (surfacePrivate->subsurface.transaction) {
            transaction->merge(surfacePrivate->subsurface.transaction.get());
            surfacePrivate->subsurface.transaction.reset();
        }
    }
    for (SubSurfaceInterface *subsurface : std::as_const(pending->subsurface.above)) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(subsurface->surface());
        if (surfacePrivate->subsurface.transaction) {
            transaction->merge(surfacePrivate->subsurface.transaction.get());
            surfacePrivate->subsurface.transaction.reset();
        }
    }

    transaction->add(q);
    if (!sync) {
        transaction->commit();
    }

    pending->serial++;
}

void SurfaceInterfacePrivate::surface_set_buffer_transform(Resource *resource, int32_t transform)
{
    if (transform < 0 || transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
        wl_resource_post_error(resource->handle, error_invalid_transform, "buffer transform must be a valid transform (%d specified)", transform);
        return;
    }
    pending->bufferTransform = OutputTransform::Kind(transform);
    pending->bufferTransformIsSet = true;
}

void SurfaceInterfacePrivate::surface_set_buffer_scale(Resource *resource, int32_t scale)
{
    if (scale < 1) {
        wl_resource_post_error(resource->handle, error_invalid_scale, "buffer scale must be at least one (%d specified)", scale);
        return;
    }
    pending->bufferScale = scale;
    pending->bufferScaleIsSet = true;
}

void SurfaceInterfacePrivate::surface_damage_buffer(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    pending->bufferDamage += QRect(x, y, width, height);
}

void SurfaceInterfacePrivate::surface_offset(Resource *resource, int32_t x, int32_t y)
{
    pending->offset = QPoint(x, y);
}

SurfaceInterface::SurfaceInterface(CompositorInterface *compositor, wl_resource *resource)
    : QObject(compositor)
    , d(new SurfaceInterfacePrivate(this))
{
    d->compositor = compositor;
    d->init(resource);
    d->client = compositor->display()->getConnection(d->resource()->client());

    d->pendingScaleOverride = d->client->scaleOverride();
    d->scaleOverride = d->pendingScaleOverride;
    connect(d->client, &ClientConnection::scaleOverrideChanged, this, [this]() {
        d->pendingScaleOverride = d->client->scaleOverride();
    });
}

SurfaceInterface::~SurfaceInterface()
{
}

SurfaceRole *SurfaceInterface::role() const
{
    return d->role;
}

void SurfaceInterface::setRole(SurfaceRole *role)
{
    d->role = role;
}

uint32_t SurfaceInterface::id() const
{
    return wl_resource_get_id(resource());
}

ClientConnection *SurfaceInterface::client() const
{
    return d->client;
}

wl_resource *SurfaceInterface::resource() const
{
    return d->resource()->handle;
}

CompositorInterface *SurfaceInterface::compositor() const
{
    return d->compositor;
}

void SurfaceInterface::frameRendered(quint32 msec)
{
    // notify all callbacks
    wl_resource *resource;
    wl_resource *tmp;

    wl_resource_for_each_safe (resource, tmp, &d->current->frameCallbacks) {
        wl_callback_send_done(resource, msec);
        wl_resource_destroy(resource);
    }
}

std::unique_ptr<PresentationFeedback> SurfaceInterface::takePresentationFeedback(Output *output)
{
    if (output && (!d->primaryOutput || d->primaryOutput->handle() != output)) {
        return nullptr;
    }
    return std::move(d->current->presentationFeedback);
}

bool SurfaceInterface::hasFrameCallbacks() const
{
    return !wl_list_empty(&d->current->frameCallbacks);
}

QRectF SurfaceInterfacePrivate::computeBufferSourceBox() const
{
    if (!current->viewport.sourceGeometry.isValid()) {
        return QRectF(QPointF(0, 0), current->buffer->size());
    }

    const QSizeF bounds = current->bufferTransform.map(current->buffer->size());
    const QRectF box(current->viewport.sourceGeometry.x() * current->bufferScale,
                     current->viewport.sourceGeometry.y() * current->bufferScale,
                     current->viewport.sourceGeometry.width() * current->bufferScale,
                     current->viewport.sourceGeometry.height() * current->bufferScale);

    return current->bufferTransform.map(box, bounds);
}

SurfaceState::SurfaceState()
{
    wl_list_init(&frameCallbacks);
}

SurfaceState::~SurfaceState()
{
    wl_resource *resource;
    wl_resource *tmp;

    wl_resource_for_each_safe (resource, tmp, &frameCallbacks) {
        wl_resource_destroy(resource);
    }
}

void SurfaceState::mergeInto(SurfaceState *target)
{
    target->serial = serial;

    if (bufferIsSet) {
        target->buffer = buffer;
        target->offset = offset;
        target->damage = damage;
        target->bufferDamage = bufferDamage;
        target->acquirePoint.timeline = std::exchange(acquirePoint.timeline, nullptr);
        target->acquirePoint.point = acquirePoint.point;
        target->releasePoint = std::move(releasePoint);
        target->bufferIsSet = true;
    }
    if (viewport.sourceGeometryIsSet) {
        target->viewport.sourceGeometry = viewport.sourceGeometry;
        target->viewport.sourceGeometryIsSet = true;
    }
    if (viewport.destinationSizeIsSet) {
        target->viewport.destinationSize = viewport.destinationSize;
        target->viewport.destinationSizeIsSet = true;
    }

    target->subsurface = subsurface;
    target->subsurfaceOrderChanged = subsurfaceOrderChanged;
    target->subsurfacePositionChanged = subsurfacePositionChanged;

    wl_list_insert_list(&target->frameCallbacks, &frameCallbacks);

    if (shadowIsSet) {
        target->shadow = shadow;
        target->shadowIsSet = true;
    }
    if (blurIsSet) {
        target->blur = blur;
        target->blurIsSet = true;
    }
    if (contrastIsSet) {
        target->contrast = contrast;
        target->contrastIsSet = true;
    }
    if (slideIsSet) {
        target->slide = slide;
        target->slideIsSet = true;
    }
    if (inputIsSet) {
        target->input = input;
        target->inputIsSet = true;
    }
    if (opaqueIsSet) {
        target->opaque = opaque;
        target->opaqueIsSet = true;
    }
    if (bufferScaleIsSet) {
        target->bufferScale = bufferScale;
        target->bufferScaleIsSet = true;
    }
    if (bufferTransformIsSet) {
        target->bufferTransform = bufferTransform;
        target->bufferTransformIsSet = true;
    }
    if (contentTypeIsSet) {
        target->contentType = contentType;
        target->contentTypeIsSet = true;
    }
    if (presentationModeHintIsSet) {
        target->presentationHint = presentationHint;
        target->presentationModeHintIsSet = true;
    }
    if (colorDescriptionIsSet) {
        target->colorDescription = colorDescription;
        target->colorDescriptionIsSet = true;
    }
    if (alphaMultiplierIsSet) {
        target->alphaMultiplier = alphaMultiplier;
        target->alphaMultiplierIsSet = true;
    }
    target->presentationFeedback = std::move(presentationFeedback);

    *this = SurfaceState{};
    serial = target->serial;
    subsurface = target->subsurface;
    wl_list_init(&frameCallbacks);
}

void SurfaceInterfacePrivate::applyState(SurfaceState *next)
{
    const bool bufferChanged = next->bufferIsSet && (current->buffer != next->buffer);
    const bool opaqueRegionChanged = next->opaqueIsSet;
    const bool transformChanged = next->bufferTransformIsSet && (current->bufferTransform != next->bufferTransform);
    const bool shadowChanged = next->shadowIsSet;
    const bool blurChanged = next->blurIsSet;
    const bool contrastChanged = next->contrastIsSet;
    const bool slideChanged = next->slideIsSet;
    const bool subsurfaceOrderChanged = next->subsurfaceOrderChanged;
    const bool visibilityChanged = next->bufferIsSet && bool(current->buffer) != bool(next->buffer);
    const bool colorDescriptionChanged = next->colorDescriptionIsSet;
    const bool presentationModeHintChanged = next->presentationModeHintIsSet;
    const bool bufferReleasePointChanged = next->bufferIsSet && current->releasePoint != next->releasePoint;
    const bool alphaMultiplierChanged = next->alphaMultiplierIsSet;

    const QSizeF oldSurfaceSize = surfaceSize;
    const QRectF oldBufferSourceBox = bufferSourceBox;
    const QRegion oldInputRegion = inputRegion;

    next->mergeInto(current.get());
    bufferRef = current->buffer;
    if (bufferRef && current->releasePoint) {
        bufferRef->addReleasePoint(current->releasePoint);
    }
    scaleOverride = pendingScaleOverride;

    if (current->buffer) {
        bufferSourceBox = computeBufferSourceBox();

        if (current->viewport.destinationSize.isValid()) {
            surfaceSize = current->viewport.destinationSize;
        } else if (current->viewport.sourceGeometry.isValid()) {
            surfaceSize = current->viewport.sourceGeometry.size();
        } else {
            surfaceSize = current->bufferTransform.map(current->buffer->size() / current->bufferScale);
        }

        const QRectF surfaceRect(QPoint(0, 0), surfaceSize);
        inputRegion = current->input & surfaceRect.toAlignedRect();

        if (!current->buffer->hasAlphaChannel()) {
            opaqueRegion = surfaceRect.toAlignedRect();
        } else {
            opaqueRegion = current->opaque & surfaceRect.toAlignedRect();
        }

        QMatrix4x4 scaleOverrideMatrix;
        if (scaleOverride != 1.) {
            scaleOverrideMatrix.scale(1. / scaleOverride, 1. / scaleOverride);
        }

        opaqueRegion = map_helper(scaleOverrideMatrix, opaqueRegion);
        inputRegion = map_helper(scaleOverrideMatrix, inputRegion);
        surfaceSize = surfaceSize / scaleOverride;
    } else {
        surfaceSize = QSizeF(0, 0);
        bufferSourceBox = QRectF();
        inputRegion = QRegion();
        opaqueRegion = QRegion();
    }

    if (opaqueRegionChanged) {
        Q_EMIT q->opaqueChanged(opaqueRegion);
    }
    if (oldInputRegion != inputRegion) {
        Q_EMIT q->inputChanged(inputRegion);
    }
    if (transformChanged) {
        Q_EMIT q->bufferTransformChanged(current->bufferTransform);
    }
    if (visibilityChanged) {
        updateEffectiveMapped();
    }
    if (bufferSourceBox != oldBufferSourceBox) {
        Q_EMIT q->bufferSourceBoxChanged();
    }
    if (bufferChanged) {
        Q_EMIT q->bufferChanged();
    }
    if (surfaceSize != oldSurfaceSize) {
        Q_EMIT q->sizeChanged();
    }
    if (shadowChanged) {
        Q_EMIT q->shadowChanged();
    }
    if (blurChanged) {
        Q_EMIT q->blurChanged();
    }
    if (contrastChanged) {
        Q_EMIT q->contrastChanged();
    }
    if (slideChanged) {
        Q_EMIT q->slideOnShowHideChanged();
    }
    if (subsurfaceOrderChanged) {
        Q_EMIT q->childSubSurfacesChanged();
    }
    if (colorDescriptionChanged) {
        Q_EMIT q->colorDescriptionChanged();
    }
    if (presentationModeHintChanged) {
        Q_EMIT q->presentationModeHintChanged();
    }
    if (bufferReleasePointChanged) {
        Q_EMIT q->bufferReleasePointChanged();
    }
    if (alphaMultiplierChanged) {
        Q_EMIT q->alphaMultiplierChanged();
    }

    if (bufferChanged) {
        if (current->buffer && (!current->damage.isEmpty() || !current->bufferDamage.isEmpty())) {
            const QRect bufferRect = QRect(QPoint(0, 0), current->buffer->size());
            bufferDamage = current->bufferDamage
                               .united(mapToBuffer(current->damage))
                               .intersected(bufferRect);
            Q_EMIT q->damaged(bufferDamage);
        }
    }

    // The position of a sub-surface is applied when its parent is committed.
    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.below)) {
        subsurface->parentApplyState(next->serial);
    }
    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.above)) {
        subsurface->parentApplyState(next->serial);
    }

    Q_EMIT q->stateApplied(next->serial);
    Q_EMIT q->committed();
}

bool SurfaceInterfacePrivate::computeEffectiveMapped() const
{
    if (!bufferRef) {
        return false;
    }
    if (subsurface.handle) {
        return subsurface.handle->parentSurface() && subsurface.handle->parentSurface()->isMapped();
    }
    return true;
}

void SurfaceInterfacePrivate::updateEffectiveMapped()
{
    const bool effectiveMapped = computeEffectiveMapped();
    if (mapped == effectiveMapped) {
        return;
    }

    mapped = effectiveMapped;

    if (mapped) {
        Q_EMIT q->mapped();
    } else {
        Q_EMIT q->unmapped();
    }

    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.below)) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(subsurface->surface());
        surfacePrivate->updateEffectiveMapped();
    }
    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.above)) {
        auto surfacePrivate = SurfaceInterfacePrivate::get(subsurface->surface());
        surfacePrivate->updateEffectiveMapped();
    }
}

bool SurfaceInterfacePrivate::contains(const QPointF &position) const
{
    // avoid QRectF::contains as that includes all edges
    const qreal x = position.x();
    const qreal y = position.y();

    return mapped && x >= 0 && y >= 0 && x < surfaceSize.width() && y < surfaceSize.height();
}

bool SurfaceInterfacePrivate::inputContains(const QPointF &position) const
{
    return contains(position) && inputRegion.contains(QPoint(std::floor(position.x()), std::floor(position.y())));
}

QRegion SurfaceInterfacePrivate::mapToBuffer(const QRegion &region) const
{
    if (region.isEmpty()) {
        return QRegion();
    }

    const QRectF sourceBox = current->bufferTransform.inverted().map(bufferSourceBox, current->buffer->size());
    const qreal xScale = sourceBox.width() / surfaceSize.width();
    const qreal yScale = sourceBox.height() / surfaceSize.height();

    QRegion result;
    for (QRectF rect : region) {
        result += current->bufferTransform.map(QRectF(rect.x() * xScale, rect.y() * yScale, rect.width() * xScale, rect.height() * yScale), sourceBox.size()).translated(bufferSourceBox.topLeft()).toAlignedRect();
    }
    return result;
}

QRegion SurfaceInterface::bufferDamage() const
{
    return d->bufferDamage;
}

QRegion SurfaceInterface::opaque() const
{
    return d->opaqueRegion;
}

QRegion SurfaceInterface::input() const
{
    return d->inputRegion;
}

QRectF SurfaceInterface::bufferSourceBox() const
{
    return d->bufferSourceBox;
}

OutputTransform SurfaceInterface::bufferTransform() const
{
    return d->current->bufferTransform;
}

GraphicsBuffer *SurfaceInterface::buffer() const
{
    return d->bufferRef.buffer();
}

QPoint SurfaceInterface::offset() const
{
    return d->current->offset / d->scaleOverride;
}

SurfaceInterface *SurfaceInterface::get(wl_resource *native)
{
    if (auto surfacePrivate = resource_cast<SurfaceInterfacePrivate *>(native)) {
        return surfacePrivate->q;
    }
    return nullptr;
}

SurfaceInterface *SurfaceInterface::get(quint32 id, const ClientConnection *client)
{
    if (client) {
        return get(client->getResource(id));
    }
    return nullptr;
}

QList<SubSurfaceInterface *> SurfaceInterface::below() const
{
    return d->current->subsurface.below;
}

QList<SubSurfaceInterface *> SurfaceInterface::above() const
{
    return d->current->subsurface.above;
}

SubSurfaceInterface *SurfaceInterface::subSurface() const
{
    return d->subsurface.handle;
}

SurfaceInterface *SurfaceInterface::mainSurface()
{
    return subSurface() ? subSurface()->mainSurface() : this;
}

QSizeF SurfaceInterface::size() const
{
    return d->surfaceSize;
}

QRectF SurfaceInterface::boundingRect() const
{
    QRectF rect(QPoint(0, 0), size());

    for (const SubSurfaceInterface *subSurface : std::as_const(d->current->subsurface.below)) {
        const SurfaceInterface *childSurface = subSurface->surface();
        rect |= childSurface->boundingRect().translated(subSurface->position());
    }
    for (const SubSurfaceInterface *subSurface : std::as_const(d->current->subsurface.above)) {
        const SurfaceInterface *childSurface = subSurface->surface();
        rect |= childSurface->boundingRect().translated(subSurface->position());
    }

    return rect;
}

ShadowInterface *SurfaceInterface::shadow() const
{
    return d->current->shadow;
}

BlurInterface *SurfaceInterface::blur() const
{
    return d->current->blur;
}

ContrastInterface *SurfaceInterface::contrast() const
{
    return d->current->contrast;
}

SlideInterface *SurfaceInterface::slideOnShowHide() const
{
    return d->current->slide;
}

bool SurfaceInterface::isMapped() const
{
    return d->mapped;
}

QList<OutputInterface *> SurfaceInterface::outputs() const
{
    return d->outputs;
}

void SurfaceInterface::setOutputs(const QList<OutputInterface *> &outputs, OutputInterface *primaryOutput)
{
    if (d->outputs == outputs && d->primaryOutput == primaryOutput) {
        return;
    }

    QList<OutputInterface *> removedOutputs = d->outputs;
    for (auto it = outputs.constBegin(), end = outputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        removedOutputs.removeOne(o);
    }
    for (auto it = removedOutputs.constBegin(), end = removedOutputs.constEnd(); it != end; ++it) {
        const auto resources = (*it)->clientResources(client()->client());
        for (wl_resource *outputResource : resources) {
            d->send_leave(outputResource);
        }
        disconnect(d->outputDestroyedConnections.take(*it));
        disconnect(d->outputBoundConnections.take(*it));
    }
    QList<OutputInterface *> addedOutputsOutputs = outputs;
    for (auto it = d->outputs.constBegin(), end = d->outputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        addedOutputsOutputs.removeOne(o);
    }
    for (auto it = addedOutputsOutputs.constBegin(), end = addedOutputsOutputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        const auto resources = o->clientResources(client()->client());
        for (wl_resource *outputResource : resources) {
            d->send_enter(outputResource);
        }
        d->outputDestroyedConnections[o] = connect(o, &OutputInterface::removed, this, [this, o] {
            auto outputs = d->outputs;
            if (outputs.removeOne(o)) {
                setOutputs(outputs, d->primaryOutput);
            }
        });

        Q_ASSERT(!d->outputBoundConnections.contains(o));
        d->outputBoundConnections[o] = connect(o, &OutputInterface::bound, this, [this](ClientConnection *c, wl_resource *outputResource) {
            if (c != client()) {
                return;
            }
            d->send_enter(outputResource);
        });
    }

    d->outputs = outputs;
    d->primaryOutput = primaryOutput;
    for (auto child : std::as_const(d->current->subsurface.below)) {
        child->surface()->setOutputs(outputs, primaryOutput);
    }
    for (auto child : std::as_const(d->current->subsurface.above)) {
        child->surface()->setOutputs(outputs, primaryOutput);
    }
}

SurfaceInterface *SurfaceInterface::surfaceAt(const QPointF &position)
{
    if (!isMapped()) {
        return nullptr;
    }

    for (auto it = d->current->subsurface.above.crbegin(); it != d->current->subsurface.above.crend(); ++it) {
        const SubSurfaceInterface *current = *it;
        SurfaceInterface *surface = current->surface();
        if (auto s = surface->surfaceAt(position - current->position())) {
            return s;
        }
    }

    // check whether the geometry contains the pos
    if (d->contains(position)) {
        return this;
    }

    for (auto it = d->current->subsurface.below.crbegin(); it != d->current->subsurface.below.crend(); ++it) {
        const SubSurfaceInterface *current = *it;
        SurfaceInterface *surface = current->surface();
        if (auto s = surface->surfaceAt(position - current->position())) {
            return s;
        }
    }
    return nullptr;
}

SurfaceInterface *SurfaceInterface::inputSurfaceAt(const QPointF &position)
{
    // TODO: Most of this is very similar to SurfaceInterface::surfaceAt
    //       Is there a way to reduce the code duplication?
    if (!isMapped()) {
        return nullptr;
    }

    for (auto it = d->current->subsurface.above.crbegin(); it != d->current->subsurface.above.crend(); ++it) {
        const SubSurfaceInterface *current = *it;
        auto surface = current->surface();
        if (auto s = surface->inputSurfaceAt(position - current->position())) {
            return s;
        }
    }

    // check whether the geometry and input region contain the pos
    if (d->inputContains(position)) {
        return this;
    }

    for (auto it = d->current->subsurface.below.crbegin(); it != d->current->subsurface.below.crend(); ++it) {
        const SubSurfaceInterface *current = *it;
        auto surface = current->surface();
        if (auto s = surface->inputSurfaceAt(position - current->position())) {
            return s;
        }
    }

    return nullptr;
}

LockedPointerV1Interface *SurfaceInterface::lockedPointer() const
{
    return d->lockedPointer;
}

ConfinedPointerV1Interface *SurfaceInterface::confinedPointer() const
{
    return d->confinedPointer;
}

bool SurfaceInterface::inhibitsIdle() const
{
    return !d->idleInhibitors.isEmpty();
}

LinuxDmaBufV1Feedback *SurfaceInterface::dmabufFeedbackV1() const
{
    return d->dmabufFeedbackV1.get();
}

ContentType SurfaceInterface::contentType() const
{
    return d->current->contentType;
}

QPointF SurfaceInterface::mapToChild(SurfaceInterface *child, const QPointF &point) const
{
    QPointF local = point;
    SurfaceInterface *surface = child;

    while (true) {
        if (surface == this) {
            return local;
        }

        SubSurfaceInterface *subsurface = surface->subSurface();
        if (Q_UNLIKELY(!subsurface)) {
            return QPointF();
        }

        local -= subsurface->position();
        surface = subsurface->parentSurface();
    }

    return QPointF();
}

qreal SurfaceInterface::scaleOverride() const
{
    return d->scaleOverride;
}

QPoint SurfaceInterface::toSurfaceLocal(const QPoint &point) const
{
    return QPoint(point.x() * d->scaleOverride, point.y() * d->scaleOverride);
}

QPointF SurfaceInterface::toSurfaceLocal(const QPointF &point) const
{
    return QPointF(point.x() * d->scaleOverride, point.y() * d->scaleOverride);
}

PresentationModeHint SurfaceInterface::presentationModeHint() const
{
    return d->current->presentationHint;
}

const ColorDescription &SurfaceInterface::colorDescription() const
{
    return d->current->colorDescription;
}

RenderingIntent SurfaceInterface::renderingIntent() const
{
    return d->current->renderingIntent;
}

void SurfaceInterface::setPreferredColorDescription(const ColorDescription &descr)
{
    if (d->preferredColorDescription == descr) {
        return;
    }
    d->preferredColorDescription = descr;
    if (d->frogColorManagement) {
        d->frogColorManagement->setPreferredColorDescription(descr);
    }
    for (const auto feedbackSurface : std::as_const(d->colorFeedbackSurfaces)) {
        feedbackSurface->setPreferredColorDescription(descr);
    }
    for (auto child : std::as_const(d->current->subsurface.below)) {
        child->surface()->setPreferredColorDescription(descr);
    }
    for (auto child : std::as_const(d->current->subsurface.above)) {
        child->surface()->setPreferredColorDescription(descr);
    }
}

void SurfaceInterface::setPreferredBufferScale(qreal scale)
{
    if (scale == d->preferredBufferScale) {
        return;
    }
    d->preferredBufferScale = scale;

    if (d->fractionalScaleExtension) {
        d->fractionalScaleExtension->setPreferredScale(scale);
    }
    if (d->resource()->version() >= WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION) {
        d->send_preferred_buffer_scale(std::ceil(scale));
    }

    for (auto child : std::as_const(d->current->subsurface.below)) {
        child->surface()->setPreferredBufferScale(scale);
    }
    for (auto child : std::as_const(d->current->subsurface.above)) {
        child->surface()->setPreferredBufferScale(scale);
    }
}

void SurfaceInterface::setPreferredBufferTransform(OutputTransform transform)
{
    if (transform == d->preferredBufferTransform) {
        return;
    }
    d->preferredBufferTransform = transform;

    if (d->resource()->version() >= WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION) {
        d->send_preferred_buffer_transform(uint32_t(transform.kind()));
    }

    for (auto child : std::as_const(d->current->subsurface.below)) {
        child->surface()->setPreferredBufferTransform(transform);
    }
    for (auto child : std::as_const(d->current->subsurface.above)) {
        child->surface()->setPreferredBufferTransform(transform);
    }
}

Transaction *SurfaceInterface::firstTransaction() const
{
    return d->firstTransaction;
}

void SurfaceInterface::setFirstTransaction(Transaction *transaction)
{
    d->firstTransaction = transaction;
}

Transaction *SurfaceInterface::lastTransaction() const
{
    return d->lastTransaction;
}

void SurfaceInterface::setLastTransaction(Transaction *transaction)
{
    d->lastTransaction = transaction;
}

void SurfaceInterface::traverseTree(std::function<void(SurfaceInterface *surface)> callback)
{
    callback(this);

    for (SubSurfaceInterface *subsurface : std::as_const(d->current->subsurface.below)) {
        subsurface->surface()->traverseTree(callback);
    }
    for (SubSurfaceInterface *subsurface : std::as_const(d->current->subsurface.above)) {
        subsurface->surface()->traverseTree(callback);
    }
}

std::shared_ptr<SyncReleasePoint> SurfaceInterface::bufferReleasePoint() const
{
    return d->current->releasePoint;
}

double SurfaceInterface::alphaMultiplier() const
{
    return d->current->alphaMultiplier;
}

} // namespace KWin

#include "moc_surface.cpp"
