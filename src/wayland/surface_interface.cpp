/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "surface_interface.h"
#include "surface_interface_p.h"
#include "buffer_interface.h"
#include "clientconnection.h"
#include "compositor_interface.h"
#include "idleinhibit_interface_p.h"
#include "pointerconstraints_interface_p.h"
#include "region_interface.h"
#include "subcompositor_interface.h"
#include "subsurface_interface_p.h"
#include "surfacerole_p.h"
// Qt
#include <QListIterator>
// Wayland
#include <wayland-server.h>
// std
#include <algorithm>

namespace KWaylandServer
{

SurfaceInterface::Private::Private(SurfaceInterface *q, CompositorInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &wl_surface_interface, &s_interface)
{
}

SurfaceInterface::Private::~Private()
{
    destroy();
}

void SurfaceInterface::Private::addChild(QPointer< SubSurfaceInterface > child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    pending.children.append(child);
    subSurfacePending.children.append(child);
    current.children.append(child);
    Q_Q(SurfaceInterface);
    emit q->childSubSurfaceAdded(child);
    emit q->subSurfaceTreeChanged();
    QObject::connect(child.data(), &SubSurfaceInterface::positionChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::damaged, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::unmapped, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::subSurfaceTreeChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
}

void SurfaceInterface::Private::removeChild(QPointer< SubSurfaceInterface > child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    pending.children.removeAll(child);
    subSurfacePending.children.removeAll(child);
    current.children.removeAll(child);
    Q_Q(SurfaceInterface);
    emit q->childSubSurfaceRemoved(child);
    emit q->subSurfaceTreeChanged();
    QObject::disconnect(child.data(), &SubSurfaceInterface::positionChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    if (!child->surface().isNull()) {
        QObject::disconnect(child->surface().data(), &SurfaceInterface::damaged, q, &SurfaceInterface::subSurfaceTreeChanged);
        QObject::disconnect(child->surface().data(), &SurfaceInterface::unmapped, q, &SurfaceInterface::subSurfaceTreeChanged);
        QObject::disconnect(child->surface().data(), &SurfaceInterface::subSurfaceTreeChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    }
}

bool SurfaceInterface::Private::raiseChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
    Q_Q(SurfaceInterface);
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);
    if (it == pending.children.end()) {
        return false;
    }
    if (pending.children.count() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == q) {
        // it's to the parent, so needs to become last item
        pending.children.append(*it);
        pending.children.erase(it);
        pending.childrenChanged = true;
        return true;
    }
    if (!sibling->subSurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.children.erase(it);
    // find the iterator again
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    pending.children.insert(++siblingIt, value);
    pending.childrenChanged = true;
    return true;
}

bool SurfaceInterface::Private::lowerChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
    Q_Q(SurfaceInterface);
    auto it = std::find(pending.children.begin(), pending.children.end(), subsurface);
    if (it == pending.children.end()) {
        return false;
    }
    if (pending.children.count() == 1) {
        // nothing to do
        return true;
    }
    if (sibling == q) {
        // it's to the parent, so needs to become first item
        auto value = *it;
        pending.children.erase(it);
        pending.children.prepend(value);
        pending.childrenChanged = true;
        return true;
    }
    if (!sibling->subSurface()) {
        // not a sub surface
        return false;
    }
    auto siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    if (siblingIt == pending.children.end() || siblingIt == it) {
        // not a sibling
        return false;
    }
    auto value = (*it);
    pending.children.erase(it);
    // find the iterator again
    siblingIt = std::find(pending.children.begin(), pending.children.end(), sibling->subSurface());
    pending.children.insert(siblingIt, value);
    pending.childrenChanged = true;
    return true;
}

void SurfaceInterface::Private::setShadow(const QPointer<ShadowInterface> &shadow)
{
    pending.shadow = shadow;
    pending.shadowIsSet = true;
}

void SurfaceInterface::Private::setBlur(const QPointer<BlurInterface> &blur)
{
    pending.blur = blur;
    pending.blurIsSet = true;
}

void SurfaceInterface::Private::setSlide(const QPointer<SlideInterface> &slide)
{
    pending.slide = slide;
    pending.slideIsSet = true;
}

void SurfaceInterface::Private::setContrast(const QPointer<ContrastInterface> &contrast)
{
    pending.contrast = contrast;
    pending.contrastIsSet = true;
}

void SurfaceInterface::Private::installPointerConstraint(LockedPointerInterface *lock)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    lockedPointer = QPointer<LockedPointerInterface>(lock);

    auto cleanUp = [this]() {
        lockedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        emit q_func()->pointerConstraintsChanged();
    };

    if (lock->lifeTime() == LockedPointerInterface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(lock, &LockedPointerInterface::lockedChanged, q_func(),
            [this, cleanUp] {
                if (lockedPointer.isNull() || lockedPointer->isLocked()) {
                    return;
                }
                cleanUp();
            }
        );
    }
    constrainsUnboundConnection = QObject::connect(lock, &LockedPointerInterface::unbound, q_func(),
        [this, cleanUp] {
            if (lockedPointer.isNull()) {
                return;
            }
            cleanUp();
        }
    );
    emit q_func()->pointerConstraintsChanged();
}

void SurfaceInterface::Private::installPointerConstraint(ConfinedPointerInterface *confinement)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    confinedPointer = QPointer<ConfinedPointerInterface>(confinement);

    auto cleanUp = [this]() {
        confinedPointer.clear();
        disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        emit q_func()->pointerConstraintsChanged();
    };

    if (confinement->lifeTime() == ConfinedPointerInterface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(confinement, &ConfinedPointerInterface::confinedChanged, q_func(),
            [this, cleanUp] {
                if (confinedPointer.isNull() || confinedPointer->isConfined()) {
                    return;
                }
                cleanUp();
            }
        );
    }
    constrainsUnboundConnection = QObject::connect(confinement, &ConfinedPointerInterface::unbound, q_func(),
        [this, cleanUp] {
            if (confinedPointer.isNull()) {
                return;
            }
            cleanUp();
        }
    );
    emit q_func()->pointerConstraintsChanged();
}

void SurfaceInterface::Private::installIdleInhibitor(IdleInhibitorInterface *inhibitor)
{
    idleInhibitors << inhibitor;
    QObject::connect(inhibitor, &IdleInhibitorInterface::aboutToBeUnbound, q,
        [this, inhibitor] {
            idleInhibitors.removeOne(inhibitor);
            if (idleInhibitors.isEmpty()) {
                emit q_func()->inhibitsIdleChanged();
            }
        }
    );
    if (idleInhibitors.count() == 1) {
        emit q_func()->inhibitsIdleChanged();
    }
}

#ifndef K_DOXYGEN
const struct wl_surface_interface SurfaceInterface::Private::s_interface = {
    resourceDestroyedCallback,
    attachCallback,
    damageCallback,
    frameCallback,
    opaqueRegionCallback,
    inputRegionCallback,
    commitCallback,
    bufferTransformCallback,
    bufferScaleCallback,
    damageBufferCallback
};
#endif

SurfaceInterface::SurfaceInterface(CompositorInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

SurfaceInterface::~SurfaceInterface() = default;

void SurfaceInterface::frameRendered(quint32 msec)
{
    Q_D();
    // notify all callbacks
    const bool needsFlush = !d->current.callbacks.isEmpty();
    while (!d->current.callbacks.isEmpty()) {
        wl_resource *r = d->current.callbacks.takeFirst();
        wl_callback_send_done(r, msec);
        wl_resource_destroy(r);
    }
    for (auto it = d->current.children.constBegin(); it != d->current.children.constEnd(); ++it) {
        const auto &subSurface = *it;
        if (subSurface.isNull() || subSurface->d_func()->surface.isNull()) {
            continue;
        }
        subSurface->d_func()->surface->frameRendered(msec);
    }
    if (needsFlush)  {
        client()->flush();
    }
}

void SurfaceInterface::Private::destroy()
{
    // copy all existing callbacks to new list and clear existing lists
    // the wl_resource_destroy on the callback resource goes into destroyFrameCallback
    // which would modify the list we are iterating on
    QList<wl_resource *> callbacksToDestroy;
    callbacksToDestroy << current.callbacks;
    current.callbacks.clear();
    callbacksToDestroy << pending.callbacks;
    pending.callbacks.clear();
    callbacksToDestroy << subSurfacePending.callbacks;
    subSurfacePending.callbacks.clear();
    for (auto it = callbacksToDestroy.constBegin(), end = callbacksToDestroy.constEnd(); it != end; it++) {
        wl_resource_destroy(*it);
    }
    if (current.buffer) {
        current.buffer->unref();
    }
}

QMatrix4x4 SurfaceInterface::Private::buildSurfaceToBufferMatrix(const State *state)
{
    // The order of transforms is reversed, i.e. the viewport transform is the first one.

    QMatrix4x4 surfaceToBufferMatrix;

    if (!state->buffer) {
        return surfaceToBufferMatrix;
    }

    surfaceToBufferMatrix.scale(state->bufferScale, state->bufferScale);

    switch (state->bufferTransform) {
    case OutputInterface::Transform::Normal:
    case OutputInterface::Transform::Flipped:
        break;
    case OutputInterface::Transform::Rotated90:
    case OutputInterface::Transform::Flipped90:
        surfaceToBufferMatrix.translate(0, state->buffer->height() / state->bufferScale);
        surfaceToBufferMatrix.rotate(-90, 0, 0, 1);
        break;
    case OutputInterface::Transform::Rotated180:
    case OutputInterface::Transform::Flipped180:
        surfaceToBufferMatrix.translate(state->buffer->width() / state->bufferScale,
                                        state->buffer->height() / state->bufferScale);
        surfaceToBufferMatrix.rotate(-180, 0, 0, 1);
        break;
    case OutputInterface::Transform::Rotated270:
    case OutputInterface::Transform::Flipped270:
        surfaceToBufferMatrix.translate(state->buffer->width() / state->bufferScale, 0);
        surfaceToBufferMatrix.rotate(-270, 0, 0, 1);
        break;
    }

    switch (state->bufferTransform) {
    case OutputInterface::Transform::Flipped:
    case OutputInterface::Transform::Flipped180:
        surfaceToBufferMatrix.translate(state->buffer->width() / state->bufferScale, 0);
        surfaceToBufferMatrix.scale(-1, 1);
        break;
    case OutputInterface::Transform::Flipped90:
    case OutputInterface::Transform::Flipped270:
        surfaceToBufferMatrix.translate(state->buffer->height() / state->bufferScale, 0);
        surfaceToBufferMatrix.scale(-1, 1);
        break;
    default:
        break;
    }

    if (state->sourceGeometry.isValid()) {
        surfaceToBufferMatrix.translate(state->sourceGeometry.x(), state->sourceGeometry.y());
        surfaceToBufferMatrix.scale(state->sourceGeometry.width() / state->size.width(),
                                    state->sourceGeometry.height() / state->size.height());
    }

    return surfaceToBufferMatrix;
}

void SurfaceInterface::Private::swapStates(State *source, State *target, bool emitChanged)
{
    Q_Q(SurfaceInterface);
    const bool bufferChanged = source->bufferIsSet;
    const bool opaqueRegionChanged = source->opaqueIsSet;
    const bool inputRegionChanged = source->inputIsSet;
    const bool scaleFactorChanged = source->bufferScaleIsSet && (target->bufferScale != source->bufferScale);
    const bool transformChanged = source->bufferTransformIsSet && (target->bufferTransform != source->bufferTransform);
    const bool shadowChanged = source->shadowIsSet;
    const bool blurChanged = source->blurIsSet;
    const bool contrastChanged = source->contrastIsSet;
    const bool slideChanged = source->slideIsSet;
    const bool childrenChanged = source->childrenChanged;
    const bool visibilityChanged = bufferChanged && (bool(source->buffer) != bool(target->buffer));
    const QSize oldSize = target->size;
    const QMatrix4x4 oldSurfaceToBufferMatrix = surfaceToBufferMatrix;
    if (bufferChanged) {
        // TODO: is the reffing correct for subsurfaces?
        if (target->buffer) {
            if (emitChanged) {
                target->buffer->unref();
            } else {
                delete target->buffer;
                target->buffer = nullptr;
            }
        }
        if (source->buffer) {
            if (emitChanged) {
                source->buffer->ref();
            }
        }
        target->buffer = source->buffer;
        target->offset = source->offset;
        target->damage = source->damage;
        target->bufferDamage = source->bufferDamage;
        target->bufferIsSet = source->bufferIsSet;
    }
    if (source->sourceGeometryIsSet) {
        target->sourceGeometry = source->sourceGeometry;
        target->sourceGeometryIsSet = true;
    }
    if (source->destinationSizeIsSet) {
        target->destinationSize = source->destinationSize;
        target->destinationSizeIsSet = true;
    }
    if (childrenChanged) {
        target->childrenChanged = source->childrenChanged;
        target->children = source->children;
    }
    target->callbacks.append(source->callbacks);

    if (shadowChanged) {
        target->shadow = source->shadow;
        target->shadowIsSet = true;
    }
    if (blurChanged) {
        target->blur = source->blur;
        target->blurIsSet = true;
    }
    if (contrastChanged) {
        target->contrast = source->contrast;
        target->contrastIsSet = true;
    }
    if (slideChanged) {
        target->slide = source->slide;
        target->slideIsSet = true;
    }
    if (inputRegionChanged) {
        target->input = source->input;
        target->inputIsInfinite = source->inputIsInfinite;
        target->inputIsSet = true;
    }
    if (opaqueRegionChanged) {
        target->opaque = source->opaque;
        target->opaqueIsSet = true;
    }
    if (scaleFactorChanged) {
        target->bufferScale = source->bufferScale;
        target->bufferScaleIsSet = true;
    }
    if (transformChanged) {
        target->bufferTransform = source->bufferTransform;
        target->bufferTransformIsSet = true;
    }
    if (!lockedPointer.isNull()) {
        lockedPointer->d_func()->commit();
    }
    if (!confinedPointer.isNull()) {
        confinedPointer->d_func()->commit();
    }

    *source = State{};
    source->children = target->children;

    if (!emitChanged) {
        return;
    }
    // TODO: Refactor the state management code because it gets more clumsy.
    if (target->buffer) {
        if (target->destinationSize.isValid()) {
            target->size = target->destinationSize;
        } else if (target->sourceGeometry.isValid()) {
            target->size = target->sourceGeometry.size().toSize();
        } else {
            target->size = target->buffer->size() / target->bufferScale;
            switch (target->bufferTransform) {
            case OutputInterface::Transform::Rotated90:
            case OutputInterface::Transform::Rotated270:
            case OutputInterface::Transform::Flipped90:
            case OutputInterface::Transform::Flipped270:
                target->size.transpose();
                break;
            case OutputInterface::Transform::Normal:
            case OutputInterface::Transform::Rotated180:
            case OutputInterface::Transform::Flipped:
            case OutputInterface::Transform::Flipped180:
                break;
            }
        }
    } else {
        target->size = QSize();
    }
    surfaceToBufferMatrix = buildSurfaceToBufferMatrix(target);
    bufferToSurfaceMatrix = surfaceToBufferMatrix.inverted();
    if (opaqueRegionChanged) {
        emit q->opaqueChanged(target->opaque);
    }
    if (inputRegionChanged) {
        emit q->inputChanged(target->input);
    }
    if (scaleFactorChanged) {
        emit q->bufferScaleChanged(target->bufferScale);
    }
    if (transformChanged) {
        emit q->bufferTransformChanged(target->bufferTransform);
    }
    if (visibilityChanged) {
        if (target->buffer) {
            subSurfaceIsMapped = true;
            emit q->mapped();
        } else {
            subSurfaceIsMapped = false;
            emit q->unmapped();
        }
    }
    if (bufferChanged) {
        if (target->buffer && (!target->damage.isEmpty() || !target->bufferDamage.isEmpty())) {
            const QRegion windowRegion = QRegion(0, 0, q->size().width(), q->size().height());
            const QRegion bufferDamage = q->mapFromBuffer(target->bufferDamage);
            target->damage = windowRegion.intersected(target->damage.united(bufferDamage));
            trackedDamage = trackedDamage.united(target->damage);
            emit q->damaged(target->damage);
            // workaround for https://bugreports.qt.io/browse/QTBUG-52092
            // if the surface is a sub-surface, but the main surface is not yet mapped, fake frame rendered
            if (subSurface) {
                const auto mainSurface = subSurface->mainSurface();
                if (!mainSurface || !mainSurface->buffer()) {
                    q->frameRendered(0);
                }
            }
        }
    }
    if (surfaceToBufferMatrix != oldSurfaceToBufferMatrix) {
        emit q->surfaceToBufferMatrixChanged();
    }
    if (target->size != oldSize) {
        emit q->sizeChanged();
    }
    if (shadowChanged) {
        emit q->shadowChanged();
    }
    if (blurChanged) {
        emit q->blurChanged();
    }
    if (contrastChanged) {
        emit q->contrastChanged();
    }
    if (slideChanged) {
        emit q->slideOnShowHideChanged();
    }
    if (childrenChanged) {
        emit q->subSurfaceTreeChanged();
    }
}

void SurfaceInterface::Private::commit()
{
    Q_Q(SurfaceInterface);
    if (!subSurface.isNull() && subSurface->isSynchronized()) {
        swapStates(&pending, &subSurfacePending, false);
    } else {
        swapStates(&pending, &current, true);
        if (!subSurface.isNull()) {
            subSurface->d_func()->commit();
        }
        // commit all subSurfaces to apply position changes
        // "The cached state is applied to the sub-surface immediately after the parent surface's state is applied"
        for (auto it = current.children.constBegin(); it != current.children.constEnd(); ++it) {
            const auto &subSurface = *it;
            if (subSurface.isNull()) {
                continue;
            }
            subSurface->d_func()->commit();
        }
    }
    if (role) {
        role->commit();
    }
    emit q->committed();
}

void SurfaceInterface::Private::commitSubSurface()
{
    if (subSurface.isNull() || !subSurface->isSynchronized()) {
        return;
    }
    swapStates(&subSurfacePending, &current, true);
    // "The cached state is applied to the sub-surface immediately after the parent surface's state is applied"
    for (auto it = current.children.constBegin(); it != current.children.constEnd(); ++it) {
        const auto &subSurface = *it;
        if (subSurface.isNull() || !subSurface->isSynchronized()) {
            continue;
        }
        subSurface->d_func()->commit();
    }
}

void SurfaceInterface::Private::damage(const QRect &rect)
{
    pending.damage = pending.damage.united(rect);
}

void SurfaceInterface::Private::damageBuffer(const QRect &rect)
{
    pending.bufferDamage = pending.bufferDamage.united(rect);
}

void SurfaceInterface::Private::setScale(qint32 scale)
{
    pending.bufferScale = scale;
    pending.bufferScaleIsSet = true;
}

void SurfaceInterface::Private::setTransform(OutputInterface::Transform transform)
{
    pending.bufferTransform = transform;
    pending.bufferTransformIsSet = true;
}

void SurfaceInterface::Private::addFrameCallback(uint32_t callback)
{
    wl_resource *r = client->createResource(&wl_callback_interface, 1, callback);
    if (!r) {
        wl_resource_post_no_memory(resource);
        return;
    }
    wl_resource_set_implementation(r, nullptr, this, destroyFrameCallback);
    pending.callbacks << r;
}

void SurfaceInterface::Private::attachBuffer(wl_resource *buffer, const QPoint &offset)
{
    pending.bufferIsSet = true;
    pending.offset = offset;
    if (pending.buffer) {
        delete pending.buffer;
    }
    if (!buffer) {
        // got a null buffer, deletes content in next frame
        pending.buffer = nullptr;
        pending.damage = QRegion();
        pending.bufferDamage = QRegion();
        return;
    }
    Q_Q(SurfaceInterface);
    pending.buffer = new BufferInterface(buffer, q);
    QObject::connect(pending.buffer, &BufferInterface::aboutToBeDestroyed, q,
        [this](BufferInterface *buffer) {
            if (pending.buffer == buffer) {
                pending.buffer = nullptr;
            }
            if (subSurfacePending.buffer == buffer) {
                subSurfacePending.buffer = nullptr;
            }
            if (current.buffer == buffer) {
                current.buffer->unref();
                current.buffer = nullptr;
            }
        }
    );
}

void SurfaceInterface::Private::destroyFrameCallback(wl_resource *r)
{
    auto s = cast<Private>(r);
    s->current.callbacks.removeAll(r);
    s->pending.callbacks.removeAll(r);
    s->subSurfacePending.callbacks.removeAll(r);
}

void SurfaceInterface::Private::attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy)
{
    Q_UNUSED(client)
    cast<Private>(resource)->attachBuffer(buffer, QPoint(sx, sy));
}

void SurfaceInterface::Private::damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(resource)->damage(QRect(x, y, width, height));
}

void SurfaceInterface::Private::damageBufferCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(resource)->damageBuffer(QRect(x, y, width, height));
}

void SurfaceInterface::Private::frameCallback(wl_client *client, wl_resource *resource, uint32_t callback)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->addFrameCallback(callback);
}

void SurfaceInterface::Private::opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    auto r = RegionInterface::get(region);
    s->setOpaque(r ? r->region() : QRegion());
}

void SurfaceInterface::Private::setOpaque(const QRegion &region)
{
    pending.opaqueIsSet = true;
    pending.opaque = region;
}

void SurfaceInterface::Private::inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    auto r = RegionInterface::get(region);
    s->setInput(r ? r->region() : QRegion(), !r);
}

void SurfaceInterface::Private::setInput(const QRegion &region, bool isInfinite)
{
    pending.inputIsSet = true;
    pending.inputIsInfinite = isInfinite;
    pending.input = region;
}

void SurfaceInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->commit();
}

void SurfaceInterface::Private::bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setTransform(OutputInterface::Transform(transform));
}

void SurfaceInterface::Private::bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale)
{
    Q_UNUSED(client)
    cast<Private>(resource)->setScale(scale);
}

QRegion SurfaceInterface::damage() const
{
    Q_D();
    return d->current.damage;
}

QRegion SurfaceInterface::opaque() const
{
    Q_D();
    return d->current.opaque;
}

QRegion SurfaceInterface::input() const
{
    Q_D();
    return d->current.input;
}

bool SurfaceInterface::inputIsInfinite() const
{
    Q_D();
    return d->current.inputIsInfinite;
}

qint32 SurfaceInterface::bufferScale() const
{
    Q_D();
    return d->current.bufferScale;
}

OutputInterface::Transform SurfaceInterface::bufferTransform() const
{
    Q_D();
    return d->current.bufferTransform;
}

BufferInterface *SurfaceInterface::buffer()
{
    Q_D();
    return d->current.buffer;
}

QPoint SurfaceInterface::offset() const
{
    Q_D();
    return d->current.offset;
}

SurfaceInterface *SurfaceInterface::get(wl_resource *native)
{
    return Private::get<SurfaceInterface>(native);
}

SurfaceInterface *SurfaceInterface::get(quint32 id, const ClientConnection *client)
{
    return Private::get<SurfaceInterface>(id, client);
}

QList< QPointer< SubSurfaceInterface > > SurfaceInterface::childSubSurfaces() const
{
    Q_D();
    return d->current.children;
}

QPointer< SubSurfaceInterface > SurfaceInterface::subSurface() const
{
    Q_D();
    return d->subSurface;
}

QSize SurfaceInterface::size() const
{
    Q_D();
    return d->current.size;
}

QRect SurfaceInterface::boundingRect() const
{
    QRect rect(QPoint(0, 0), size());

    const QList<QPointer<SubSurfaceInterface>> subSurfaces = childSubSurfaces();
    for (const SubSurfaceInterface *subSurface : subSurfaces) {
        const SurfaceInterface *childSurface = subSurface->surface();
        rect |= childSurface->boundingRect().translated(subSurface->position());
    }

    return rect;
}

QPointer< ShadowInterface > SurfaceInterface::shadow() const
{
    Q_D();
    return d->current.shadow;
}

QPointer< BlurInterface > SurfaceInterface::blur() const
{
    Q_D();
    return d->current.blur;
}

QPointer< ContrastInterface > SurfaceInterface::contrast() const
{
    Q_D();
    return d->current.contrast;
}

QPointer< SlideInterface > SurfaceInterface::slideOnShowHide() const
{
    Q_D();
    return d->current.slide;
}

bool SurfaceInterface::isMapped() const
{
    Q_D();
    if (d->subSurface) {
        // from spec:
        // "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and the parent surface is mapped."
        return d->subSurfaceIsMapped && !d->subSurface->parentSurface().isNull() && d->subSurface->parentSurface()->isMapped();
    }
    return d->current.buffer != nullptr;
}

QRegion SurfaceInterface::trackedDamage() const
{
    Q_D();
    return d->trackedDamage;
}

void SurfaceInterface::resetTrackedDamage()
{
    Q_D();
    d->trackedDamage = QRegion();
}

QVector<OutputInterface *> SurfaceInterface::outputs() const
{
    Q_D();
    return d->outputs;
}

void SurfaceInterface::setOutputs(const QVector<OutputInterface *> &outputs)
{
    Q_D();
    QVector<OutputInterface *> removedOutputs = d->outputs;
    for (auto it = outputs.constBegin(), end = outputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        removedOutputs.removeOne(o);
    }
    for (auto it = removedOutputs.constBegin(), end = removedOutputs.constEnd(); it != end; ++it) {
        const auto resources = (*it)->clientResources(client());
        for (wl_resource *r : resources) {
            wl_surface_send_leave(d->resource, r);
        }
        disconnect(d->outputDestroyedConnections.take(*it));
    }
    QVector<OutputInterface *> addedOutputsOutputs = outputs;
    for (auto it = d->outputs.constBegin(), end = d->outputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        addedOutputsOutputs.removeOne(o);
    }
    for (auto it = addedOutputsOutputs.constBegin(), end = addedOutputsOutputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        const auto resources = o->clientResources(client());
        for (wl_resource *r : resources) {
            wl_surface_send_enter(d->resource, r);
        }
        d->outputDestroyedConnections[o] = connect(o, &Global::aboutToDestroyGlobal, this, [this, o] {
            Q_D();
            auto outputs = d->outputs;
            if (outputs.removeOne(o)) {
                setOutputs(outputs);
            }});
    }
    // TODO: send enter when the client binds the OutputInterface another time

    d->outputs = outputs;
}

SurfaceInterface *SurfaceInterface::surfaceAt(const QPointF &position)
{
    if (!isMapped()) {
        return nullptr;
    }
    Q_D();
    // go from top to bottom. Top most child is last in list
    QListIterator<QPointer<SubSurfaceInterface>> it(d->current.children);
    it.toBack();
    while (it.hasPrevious()) {
        const auto &current = it.previous();
        auto surface = current->surface();
        if (surface.isNull()) {
            continue;
        }
        if (auto s = surface->surfaceAt(position - current->position())) {
            return s;
        }
    }
    // check whether the geometry contains the pos
    if (!size().isEmpty() && QRectF(QPoint(0, 0), size()).contains(position)) {
        return this;
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
    Q_D();
    // go from top to bottom. Top most child is last in list
    QListIterator<QPointer<SubSurfaceInterface>> it(d->current.children);
    it.toBack();
    while (it.hasPrevious()) {
        const auto &current = it.previous();
        auto surface = current->surface();
        if (surface.isNull()) {
            continue;
        }
        if (auto s = surface->inputSurfaceAt(position - current->position())) {
            return s;
        }
    }
    // check whether the geometry and input region contain the pos
    if (!size().isEmpty() && QRectF(QPoint(0, 0), size()).contains(position) &&
            (inputIsInfinite() || input().contains(position.toPoint()))) {
        return this;
    }
    return nullptr;
}

QPointer<LockedPointerInterface> SurfaceInterface::lockedPointer() const
{
    Q_D();
    return d->lockedPointer;
}

QPointer<ConfinedPointerInterface> SurfaceInterface::confinedPointer() const
{
    Q_D();
    return d->confinedPointer;
}

bool SurfaceInterface::inhibitsIdle() const
{
    Q_D();
    return !d->idleInhibitors.isEmpty();
}

void SurfaceInterface::setDataProxy(SurfaceInterface *surface)
{
    Q_D();
    d->dataProxy = surface;
}

SurfaceInterface* SurfaceInterface::dataProxy() const
{
    Q_D();
    return d->dataProxy;
}

SurfaceInterface::Private *SurfaceInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QPointF SurfaceInterface::mapToBuffer(const QPointF &point) const
{
    Q_D();
    return d->surfaceToBufferMatrix.map(point);
}

QPointF SurfaceInterface::mapFromBuffer(const QPointF &point) const
{
    Q_D();
    return d->bufferToSurfaceMatrix.map(point);
}

static QRegion map_helper(const QMatrix4x4 &matrix, const QRegion &region)
{
    QRegion result;
    for (const QRect &rect : region) {
        result += matrix.mapRect(rect);
    }
    return result;
}

QRegion SurfaceInterface::mapToBuffer(const QRegion &region) const
{
    Q_D();
    return map_helper(d->surfaceToBufferMatrix, region);
}

QRegion SurfaceInterface::mapFromBuffer(const QRegion &region) const
{
    Q_D();
    return map_helper(d->bufferToSurfaceMatrix, region);
}

QMatrix4x4 SurfaceInterface::surfaceToBufferMatrix() const
{
    Q_D();
    return d->surfaceToBufferMatrix;
}

}
