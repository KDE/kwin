/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "surface.h"
#include "blur.h"
#include "clientconnection.h"
#include "colormanagement_v1.h"
#include "colorrepresentation_v1.h"
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

#include <algorithm>
#include <cmath>
#include <drm_fourcc.h>
#include <wayland-server.h>

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
    pending->committed |= SurfaceState::Field::SubsurfaceOrder;
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
    pending->committed |= SurfaceState::Field::SubsurfaceOrder;
    return true;
}

void SurfaceInterfacePrivate::setShadow(const QPointer<ShadowInterface> &shadow)
{
    pending->shadow = shadow;
    pending->committed |= SurfaceState::Field::Shadow;
}

void SurfaceInterfacePrivate::setBlur(const QPointer<BlurInterface> &blur)
{
    pending->blur = blur;
    pending->committed |= SurfaceState::Field::Blur;
}

void SurfaceInterfacePrivate::setSlide(const QPointer<SlideInterface> &slide)
{
    pending->slide = slide;
    pending->committed |= SurfaceState::Field::Slide;
}

void SurfaceInterfacePrivate::setContrast(const QPointer<ContrastInterface> &contrast)
{
    pending->contrast = contrast;
    pending->committed |= SurfaceState::Field::Contrast;
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

    pending->committed |= SurfaceState::Field::Buffer;
    if (!buffer) {
        pending->buffer = nullptr;
    } else {
        pending->buffer = Display::bufferForResource(buffer);
    }
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
    pending->committed |= SurfaceState::Field::Opaque;
}

void SurfaceInterfacePrivate::surface_set_input_region(Resource *resource, struct ::wl_resource *region)
{
    RegionInterface *r = RegionInterface::get(region);
    pending->input = r ? r->region() : infiniteRegion();
    pending->committed |= SurfaceState::Field::Input;
}

void SurfaceInterfacePrivate::surface_commit(Resource *resource)
{
    const bool sync = subsurface.handle && subsurface.handle->isSynchronized();

    if (syncObjV1 && syncObjV1->maybeEmitProtocolErrors()) {
        return;
    }
    if (colorRepresentation && colorRepresentation->maybeEmitProtocolErrors()) {
        return;
    }

    if ((pending->committed & SurfaceState::Field::Buffer) && !pending->buffer) {
        pending->damage = QRegion();
        pending->bufferDamage = QRegion();
    }

    // unless a protocol overrides the properties, we need to assume some YUV->RGB conversion
    // matrix and color space to be attached to YUV formats
    const bool hasColorManagementProtocol = colorSurface || frogColorManagement;
    const bool hasColorRepresentation = colorRepresentation != nullptr;
    if (pending->buffer && pending->buffer->dmabufAttributes()) {
        switch (pending->buffer->dmabufAttributes()->format) {
        case DRM_FORMAT_NV12:
            if (!hasColorRepresentation) {
                pending->yuvCoefficients = YUVMatrixCoefficients::BT709;
                pending->range = EncodingRange::Limited;
                pending->committed |= SurfaceState::Field::YuvCoefficients;
            }
            if (!hasColorManagementProtocol) {
                pending->colorDescription = ColorDescription::sRGB;
                pending->committed |= SurfaceState::Field::ColorDescription;
            }
            break;
        case DRM_FORMAT_P010:
            if (!hasColorRepresentation) {
                pending->yuvCoefficients = YUVMatrixCoefficients::BT2020;
                pending->range = EncodingRange::Limited;
                pending->committed |= SurfaceState::Field::YuvCoefficients;
            }
            if (!hasColorManagementProtocol) {
                pending->colorDescription = ColorDescription(Colorimetry::BT2020, TransferFunction(TransferFunction::PerceptualQuantizer));
                pending->committed |= SurfaceState::Field::ColorDescription;
            }
            break;
        default:
            if (!hasColorRepresentation) {
                pending->yuvCoefficients = YUVMatrixCoefficients::Identity;
                pending->range = EncodingRange::Full;
                pending->committed |= SurfaceState::Field::YuvCoefficients;
            }
            if (!hasColorManagementProtocol) {
                pending->colorDescription = ColorDescription::sRGB;
                pending->committed |= SurfaceState::Field::ColorDescription;
            }
        }
    } else {
        if (!hasColorRepresentation) {
            pending->yuvCoefficients = YUVMatrixCoefficients::Identity;
            pending->range = EncodingRange::Full;
            pending->committed |= SurfaceState::Field::YuvCoefficients;
        }
        if (!hasColorManagementProtocol) {
            pending->colorDescription = ColorDescription::sRGB;
            pending->committed |= SurfaceState::Field::ColorDescription;
        }
    }

    Transaction *transaction;
    if (sync) {
        // if the surface is in effectively synchronized mode at commit time,
        // the fifo wait condition must be ignored
        pending->hasFifoWaitCondition = false;
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
}

void SurfaceInterfacePrivate::surface_set_buffer_transform(Resource *resource, int32_t transform)
{
    if (transform < 0 || transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
        wl_resource_post_error(resource->handle, error_invalid_transform, "buffer transform must be a valid transform (%d specified)", transform);
        return;
    }
    pending->bufferTransform = OutputTransform::Kind(transform);
    pending->committed |= SurfaceState::Field::BufferTransform;
}

void SurfaceInterfacePrivate::surface_set_buffer_scale(Resource *resource, int32_t scale)
{
    if (scale < 1) {
        wl_resource_post_error(resource->handle, error_invalid_scale, "buffer scale must be at least one (%d specified)", scale);
        return;
    }
    pending->bufferScale = scale;
    pending->committed |= SurfaceState::Field::BufferScale;
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
    d->client = ClientConnection::get(d->resource()->client());

    d->pendingScaleOverride = d->client->scaleOverride();
    d->scaleOverride = d->pendingScaleOverride;
    connect(d->client, &ClientConnection::scaleOverrideChanged, this, [this]() {
        d->pendingScaleOverride = d->client->scaleOverride();
    });
}

SurfaceInterface::~SurfaceInterface()
{
    d->m_tearingDown = true;
    if (d->firstTransaction) {
        d->firstTransaction->tryApply();
    }
}

RawSurfaceAttachedState *SurfaceInterface::addExtension(RawSurfaceExtension *extension)
{
    const auto &[it, inserted] = d->pending->extensions.emplace(extension, extension->createState());
    return it->second.get();
}

void SurfaceInterface::removeExtension(RawSurfaceExtension *extension)
{
    d->pending->extensions.erase(extension);

    if (d->subsurface.transaction) {
        d->subsurface.transaction->amend(this, [extension](SurfaceState *state) {
            state->extensions.erase(extension);
        });
    }

    for (auto transaction = d->firstTransaction; transaction; transaction = transaction->next(this)) {
        transaction->amend(this, [extension](SurfaceState *state) {
            state->extensions.erase(extension);
        });
    }
}

bool SurfaceInterface::tearingDown() const
{
    return d->m_tearingDown;
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

bool SurfaceInterface::hasPresentationFeedback() const
{
    return d->current->presentationFeedback.get();
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
    if (committed & SurfaceState::Field::Buffer) {
        target->buffer = buffer;
        target->offset = offset;
        target->damage = std::move(damage);
        target->bufferDamage = std::move(bufferDamage);
        target->acquirePoint = std::move(acquirePoint);
        target->releasePoint = std::move(releasePoint);
    }

    wl_list_insert_list(&target->frameCallbacks, &frameCallbacks);
    wl_list_init(&frameCallbacks);

    target->viewport.sourceGeometry = viewport.sourceGeometry;
    target->viewport.destinationSize = viewport.destinationSize;
    target->subsurface = subsurface;
    target->shadow = shadow;
    target->blur = blur;
    target->contrast = contrast;
    target->slide = slide;
    target->input = input;
    target->opaque = opaque;
    target->bufferScale = bufferScale;
    target->bufferTransform = bufferTransform;
    target->contentType = contentType;
    target->presentationHint = presentationHint;
    target->colorDescription = colorDescription;
    target->renderingIntent = renderingIntent;
    target->alphaMultiplier = alphaMultiplier;
    target->yuvCoefficients = yuvCoefficients;
    target->fifoBarrier |= std::exchange(fifoBarrier, false);
    target->hasFifoWaitCondition = std::exchange(hasFifoWaitCondition, false);
    target->yuvCoefficients = yuvCoefficients;
    target->range = range;
    target->presentationFeedback = std::move(presentationFeedback);

    auto previousExtensions = std::exchange(target->extensions, {});
    for (const auto &[extension, sourceState] : extensions) {
        std::unique_ptr<RawSurfaceAttachedState> targetState;

        if (auto it = previousExtensions.find(extension); it != previousExtensions.end()) {
            targetState = std::move(it->second);
        } else {
            targetState = extension->createState();
        }

        sourceState->mergeInto(targetState.get());
        target->extensions[extension] = std::move(targetState);
    }

    target->committed |= std::exchange(committed, SurfaceState::Fields{});
}

void SurfaceInterfacePrivate::applyState(SurfaceState *next)
{
    const bool bufferChanged = (next->committed & SurfaceState::Field::Buffer) && (current->buffer != next->buffer);
    const bool opaqueRegionChanged = (next->committed & SurfaceState::Field::Opaque);
    const bool transformChanged = (next->committed & SurfaceState::Field::BufferTransform) && (current->bufferTransform != next->bufferTransform);
    const bool shadowChanged = (next->committed & SurfaceState::Field::Shadow);
    const bool blurChanged = (next->committed & SurfaceState::Field::Blur);
    const bool contrastChanged = (next->committed & SurfaceState::Field::Contrast);
    const bool slideChanged = (next->committed & SurfaceState::Field::Slide);
    const bool subsurfaceOrderChanged = (next->committed & SurfaceState::Field::SubsurfaceOrder);
    const bool visibilityChanged = (next->committed & SurfaceState::Field::Buffer) && bool(current->buffer) != bool(next->buffer);
    const bool colorDescriptionChanged = (next->committed & SurfaceState::Field::ColorDescription)
        && (current->colorDescription != next->colorDescription || current->renderingIntent != next->renderingIntent);
    const bool presentationModeHintChanged = (next->committed & SurfaceState::Field::PresentationModeHint);
    const bool bufferReleasePointChanged = (next->committed & SurfaceState::Field::Buffer) && current->releasePoint != next->releasePoint;
    const bool alphaMultiplierChanged = (next->committed & SurfaceState::Field::AlphaMultiplier);
    const bool yuvCoefficientsChanged = (next->committed & SurfaceState::Field::YuvCoefficients) && (current->yuvCoefficients != next->yuvCoefficients);

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

        const QRect surfaceRect = QRectF(QPointF(0, 0), surfaceSize).toAlignedRect();
        const QRect bufferRect = QRect(QPoint(0, 0), current->buffer->size());

        inputRegion = current->input & surfaceRect;

        if (!current->buffer->hasAlphaChannel()) {
            opaqueRegion = surfaceRect;
        } else {
            opaqueRegion = current->opaque & surfaceRect;
        }

        bufferDamage = current->bufferDamage
                           .united(mapToBuffer(current->damage.intersected(surfaceRect)))
                           .intersected(bufferRect);
        current->damage = QRegion();
        current->bufferDamage = QRegion();

        if (scaleOverride != 1.0) {
            QMatrix4x4 scaleOverrideMatrix;
            scaleOverrideMatrix.scale(1.0 / scaleOverride, 1.0 / scaleOverride);

            opaqueRegion = map_helper(scaleOverrideMatrix, opaqueRegion);
            inputRegion = map_helper(scaleOverrideMatrix, inputRegion);
            surfaceSize = surfaceSize / scaleOverride;
        }
    } else {
        surfaceSize = QSizeF(0, 0);
        bufferSourceBox = QRectF();
        bufferDamage = QRegion();
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
    if (colorDescriptionChanged || yuvCoefficientsChanged) {
        current->colorDescription = current->colorDescription.withYuvCoefficients(current->yuvCoefficients, current->range);
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
    if (!bufferDamage.isEmpty()) {
        Q_EMIT q->damaged(bufferDamage);
    }

    // The position of a sub-surface is applied when its parent is committed.
    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.below)) {
        subsurface->parentApplyState();
    }
    for (SubSurfaceInterface *subsurface : std::as_const(current->subsurface.above)) {
        subsurface->parentApplyState();
    }

    for (const auto &[extension, state] : current->extensions) {
        extension->applyState(state.get());
    }

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

std::pair<SurfaceInterface *, QPointF> SurfaceInterface::mapToInputSurface(const QPointF &position)
{
    auto surface = inputSurfaceAt(position);
    if (!surface) {
        surface = this;
    }
    return std::make_pair(surface, mapToChild(surface, position));
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

    while (surface != this) {
        SubSurfaceInterface *subsurface = surface->subSurface();
        if (Q_UNLIKELY(!subsurface)) {
            return QPointF();
        }

        local -= subsurface->position();
        surface = subsurface->parentSurface();
    }

    return local;
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

void SurfaceInterface::clearFifoBarrier()
{
    if (d->current->fifoBarrier) {
        d->current->fifoBarrier = false;
        if (d->firstTransaction) {
            d->firstTransaction->tryApply();
        }
    }
}

bool SurfaceInterface::hasFifoBarrier() const
{
    return d->current->fifoBarrier;
}

} // namespace KWin

#include "moc_surface.cpp"
