/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "surface_interface.h"
#include "surface_interface_p.h"
#include "buffer_interface.h"
#include "clientconnection.h"
#include "compositor_interface.h"
#include "display.h"
#include "idleinhibit_interface_p.h"
#include "pointerconstraints_interface_p.h"
#include "region_interface.h"
#include "subcompositor_interface.h"
#include "subsurface_interface_p.h"
#include "surfacerole_p.h"
// std
#include <algorithm>

namespace KWaylandServer
{

QList<SurfaceInterface *> SurfaceInterfacePrivate::surfaces;

KWaylandFrameCallback::KWaylandFrameCallback(wl_resource *resource, SurfaceInterface *surface)
    : QtWaylandServer::wl_callback(resource)
    , surface(surface)
{
}

void KWaylandFrameCallback::destroy()
{
    wl_resource_destroy(resource()->handle);
}

void KWaylandFrameCallback::callback_destroy_resource(Resource *)
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->current.frameCallbacks.removeOne(this);
        surfacePrivate->pending.frameCallbacks.removeOne(this);
        surfacePrivate->cached.frameCallbacks.removeOne(this);
    }
    delete this;
}

SurfaceInterfacePrivate::SurfaceInterfacePrivate(SurfaceInterface *q)
    : q(q)
{
    surfaces.append(q);
}

SurfaceInterfacePrivate::~SurfaceInterfacePrivate()
{
    for (KWaylandFrameCallback *frameCallback : current.frameCallbacks) {
        frameCallback->destroy();
    }
    for (KWaylandFrameCallback *frameCallback : pending.frameCallbacks) {
        frameCallback->destroy();
    }
    for (KWaylandFrameCallback *frameCallback : cached.frameCallbacks) {
        frameCallback->destroy();
    }
    if (current.buffer) {
        current.buffer->unref();
    }
    surfaces.removeOne(q);
}

void SurfaceInterfacePrivate::addChild(QPointer<SubSurfaceInterface> child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    pending.children.append(child);
    cached.children.append(child);
    current.children.append(child);
    emit q->childSubSurfaceAdded(child);
    emit q->subSurfaceTreeChanged();
    QObject::connect(child.data(), &SubSurfaceInterface::positionChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::damaged, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::unmapped, q, &SurfaceInterface::subSurfaceTreeChanged);
    QObject::connect(child->surface().data(), &SurfaceInterface::subSurfaceTreeChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
}

void SurfaceInterfacePrivate::removeChild(QPointer<SubSurfaceInterface> child)
{
    // protocol is not precise on how to handle the addition of new sub surfaces
    pending.children.removeAll(child);
    cached.children.removeAll(child);
    current.children.removeAll(child);
    emit q->childSubSurfaceRemoved(child);
    emit q->subSurfaceTreeChanged();
    QObject::disconnect(child.data(), &SubSurfaceInterface::positionChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    if (!child->surface().isNull()) {
        QObject::disconnect(child->surface().data(), &SurfaceInterface::damaged, q, &SurfaceInterface::subSurfaceTreeChanged);
        QObject::disconnect(child->surface().data(), &SurfaceInterface::unmapped, q, &SurfaceInterface::subSurfaceTreeChanged);
        QObject::disconnect(child->surface().data(), &SurfaceInterface::subSurfaceTreeChanged, q, &SurfaceInterface::subSurfaceTreeChanged);
    }
}

bool SurfaceInterfacePrivate::raiseChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
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

bool SurfaceInterfacePrivate::lowerChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling)
{
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

void SurfaceInterfacePrivate::setShadow(const QPointer<ShadowInterface> &shadow)
{
    pending.shadow = shadow;
    pending.shadowIsSet = true;
}

void SurfaceInterfacePrivate::setBlur(const QPointer<BlurInterface> &blur)
{
    pending.blur = blur;
    pending.blurIsSet = true;
}

void SurfaceInterfacePrivate::setSlide(const QPointer<SlideInterface> &slide)
{
    pending.slide = slide;
    pending.slideIsSet = true;
}

void SurfaceInterfacePrivate::setContrast(const QPointer<ContrastInterface> &contrast)
{
    pending.contrast = contrast;
    pending.contrastIsSet = true;
}

void SurfaceInterfacePrivate::installPointerConstraint(LockedPointerInterface *lock)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    lockedPointer = QPointer<LockedPointerInterface>(lock);

    auto cleanUp = [this]() {
        lockedPointer.clear();
        QObject::disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        QObject::disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        emit q->pointerConstraintsChanged();
    };

    if (lock->lifeTime() == LockedPointerInterface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(lock, &LockedPointerInterface::lockedChanged, q,
            [this, cleanUp] {
                if (lockedPointer.isNull() || lockedPointer->isLocked()) {
                    return;
                }
                cleanUp();
            }
        );
    }
    constrainsUnboundConnection = QObject::connect(lock, &LockedPointerInterface::unbound, q,
        [this, cleanUp] {
            if (lockedPointer.isNull()) {
                return;
            }
            cleanUp();
        }
    );
    emit q->pointerConstraintsChanged();
}

void SurfaceInterfacePrivate::installPointerConstraint(ConfinedPointerInterface *confinement)
{
    Q_ASSERT(lockedPointer.isNull());
    Q_ASSERT(confinedPointer.isNull());
    confinedPointer = QPointer<ConfinedPointerInterface>(confinement);

    auto cleanUp = [this]() {
        confinedPointer.clear();
        QObject::disconnect(constrainsOneShotConnection);
        constrainsOneShotConnection = QMetaObject::Connection();
        QObject::disconnect(constrainsUnboundConnection);
        constrainsUnboundConnection = QMetaObject::Connection();
        emit q->pointerConstraintsChanged();
    };

    if (confinement->lifeTime() == ConfinedPointerInterface::LifeTime::OneShot) {
        constrainsOneShotConnection = QObject::connect(confinement, &ConfinedPointerInterface::confinedChanged, q,
            [this, cleanUp] {
                if (confinedPointer.isNull() || confinedPointer->isConfined()) {
                    return;
                }
                cleanUp();
            }
        );
    }
    constrainsUnboundConnection = QObject::connect(confinement, &ConfinedPointerInterface::unbound, q,
        [this, cleanUp] {
            if (confinedPointer.isNull()) {
                return;
            }
            cleanUp();
        }
    );
    emit q->pointerConstraintsChanged();
}

void SurfaceInterfacePrivate::installIdleInhibitor(IdleInhibitorInterface *inhibitor)
{
    idleInhibitors << inhibitor;
    QObject::connect(inhibitor, &IdleInhibitorInterface::aboutToBeUnbound, q,
        [this, inhibitor] {
            idleInhibitors.removeOne(inhibitor);
            if (idleInhibitors.isEmpty()) {
                emit q->inhibitsIdleChanged();
            }
        }
    );
    if (idleInhibitors.count() == 1) {
        emit q->inhibitsIdleChanged();
    }
}

void SurfaceInterfacePrivate::surface_destroy_resource(Resource *)
{
    emit q->aboutToBeUnbound();
    delete q;
}

void SurfaceInterfacePrivate::surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SurfaceInterfacePrivate::surface_attach(Resource *resource, struct ::wl_resource *buffer, int32_t x, int32_t y)
{
    Q_UNUSED(resource)
    pending.bufferIsSet = true;
    pending.offset = QPoint(x, y);
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
    pending.buffer = new BufferInterface(buffer, q);
    QObject::connect(pending.buffer, &BufferInterface::aboutToBeDestroyed, q,
        [this](BufferInterface *buffer) {
            if (pending.buffer == buffer) {
                pending.buffer = nullptr;
            }
            if (cached.buffer == buffer) {
                cached.buffer = nullptr;
            }
            if (current.buffer == buffer) {
                current.buffer->unref();
                current.buffer = nullptr;
            }
        }
    );
}

void SurfaceInterfacePrivate::surface_damage(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    pending.damage = pending.damage.united(QRect(x, y, width, height));
}

void SurfaceInterfacePrivate::surface_frame(Resource *resource, uint32_t callback)
{
    wl_resource *callbackResource = wl_resource_create(resource->client(), &wl_callback_interface,
                                                       /* version */ 1, callback);
    if (!callbackResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    pending.frameCallbacks.append(new KWaylandFrameCallback(callbackResource, q));
}

void SurfaceInterfacePrivate::surface_set_opaque_region(Resource *resource, struct ::wl_resource *region)
{
    Q_UNUSED(resource)
    RegionInterface *r = RegionInterface::get(region);
    pending.opaque = r ? r->region() : QRegion();
    pending.opaqueIsSet = true;

}

void SurfaceInterfacePrivate::surface_set_input_region(Resource *resource, struct ::wl_resource *region)
{
    Q_UNUSED(resource)
    RegionInterface *r = RegionInterface::get(region);
    pending.input = r ? r->region() : QRegion();
    pending.inputIsInfinite = !r;
    pending.inputIsSet = true;
}

void SurfaceInterfacePrivate::surface_commit(Resource *resource)
{
    Q_UNUSED(resource)
    commit();
}

void SurfaceInterfacePrivate::surface_set_buffer_transform(Resource *resource, int32_t transform)
{
    Q_UNUSED(resource)
    pending.bufferTransform = OutputInterface::Transform(transform);
    pending.bufferTransformIsSet = true;
}

void SurfaceInterfacePrivate::surface_set_buffer_scale(Resource *resource, int32_t scale)
{
    Q_UNUSED(resource)
    pending.bufferScale = scale;
    pending.bufferScaleIsSet = true;
}

void SurfaceInterfacePrivate::surface_damage_buffer(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(resource)
    pending.bufferDamage = pending.bufferDamage.united(QRect(x, y, width, height));
}

SurfaceInterface::SurfaceInterface(CompositorInterface *compositor, wl_resource *resource)
    : QObject(compositor)
    , d(new SurfaceInterfacePrivate(this))
{
    d->compositor = compositor;
    d->init(resource);
    d->client =  compositor->display()->getConnection(d->resource()->client());
}

SurfaceInterface::~SurfaceInterface()
{
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

QList<SurfaceInterface *> SurfaceInterface::surfaces()
{
    return SurfaceInterfacePrivate::surfaces;
}

void SurfaceInterface::frameRendered(quint32 msec)
{
    // notify all callbacks
    const bool needsFlush = !d->current.frameCallbacks.isEmpty();
    while (!d->current.frameCallbacks.isEmpty()) {
        KWaylandFrameCallback *frameCallback = d->current.frameCallbacks.takeFirst();
        frameCallback->send_done(msec);
        frameCallback->destroy();
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

QMatrix4x4 SurfaceInterfacePrivate::buildSurfaceToBufferMatrix(const State *state)
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

void SurfaceInterfacePrivate::swapStates(State *source, State *target, bool emitChanged)
{
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
    const QSize oldBufferSize = bufferSize;
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
    target->frameCallbacks.append(source->frameCallbacks);

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
        bufferSize = target->buffer->size();
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
        bufferSize = QSize();
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
    if (bufferSize != oldBufferSize) {
        emit q->bufferSizeChanged();
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

void SurfaceInterfacePrivate::commit()
{
    if (!subSurface.isNull() && subSurface->isSynchronized()) {
        swapStates(&pending, &cached, false);
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

void SurfaceInterfacePrivate::commitSubSurface()
{
    if (subSurface.isNull() || !subSurface->isSynchronized()) {
        return;
    }
    swapStates(&cached, &current, true);
    // "The cached state is applied to the sub-surface immediately after the parent surface's state is applied"
    for (auto it = current.children.constBegin(); it != current.children.constEnd(); ++it) {
        const auto &subSurface = *it;
        if (subSurface.isNull() || !subSurface->isSynchronized()) {
            continue;
        }
        subSurface->d_func()->commit();
    }
}

QRegion SurfaceInterface::damage() const
{
    return d->current.damage;
}

QRegion SurfaceInterface::opaque() const
{
    return d->current.opaque;
}

QRegion SurfaceInterface::input() const
{
    return d->current.input;
}

bool SurfaceInterface::inputIsInfinite() const
{
    return d->current.inputIsInfinite;
}

qint32 SurfaceInterface::bufferScale() const
{
    return d->current.bufferScale;
}

OutputInterface::Transform SurfaceInterface::bufferTransform() const
{
    return d->current.bufferTransform;
}

BufferInterface *SurfaceInterface::buffer()
{
    return d->current.buffer;
}

QPoint SurfaceInterface::offset() const
{
    return d->current.offset;
}

SurfaceInterface *SurfaceInterface::get(wl_resource *native)
{
    if (auto surface = SurfaceInterfacePrivate::Resource::fromResource(native)) {
        return static_cast<SurfaceInterfacePrivate *>(surface->object())->q;
    }
    return nullptr;
}

SurfaceInterface *SurfaceInterface::get(quint32 id, const ClientConnection *client)
{
    const QList<SurfaceInterface *> candidates = surfaces();
    for (SurfaceInterface *surface : candidates) {
        if (surface->client() == client && surface->id() == id) {
            return surface;
        }
    }
    return nullptr;
}

QList< QPointer< SubSurfaceInterface > > SurfaceInterface::childSubSurfaces() const
{
    return d->current.children;
}

QPointer< SubSurfaceInterface > SurfaceInterface::subSurface() const
{
    return d->subSurface;
}

QSize SurfaceInterface::size() const
{
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
    return d->current.shadow;
}

QPointer< BlurInterface > SurfaceInterface::blur() const
{
    return d->current.blur;
}

QPointer< ContrastInterface > SurfaceInterface::contrast() const
{
    return d->current.contrast;
}

QPointer< SlideInterface > SurfaceInterface::slideOnShowHide() const
{
    return d->current.slide;
}

bool SurfaceInterface::isMapped() const
{
    if (d->subSurface) {
        // from spec:
        // "A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and the parent surface is mapped."
        return d->subSurfaceIsMapped && !d->subSurface->parentSurface().isNull() && d->subSurface->parentSurface()->isMapped();
    }
    return d->current.buffer != nullptr;
}

QRegion SurfaceInterface::trackedDamage() const
{
    return d->trackedDamage;
}

void SurfaceInterface::resetTrackedDamage()
{
    d->trackedDamage = QRegion();
}

QVector<OutputInterface *> SurfaceInterface::outputs() const
{
    return d->outputs;
}

void SurfaceInterface::setOutputs(const QVector<OutputInterface *> &outputs)
{
    QVector<OutputInterface *> removedOutputs = d->outputs;
    for (auto it = outputs.constBegin(), end = outputs.constEnd(); it != end; ++it) {
        const auto o = *it;
        removedOutputs.removeOne(o);
    }
    for (auto it = removedOutputs.constBegin(), end = removedOutputs.constEnd(); it != end; ++it) {
        const auto resources = (*it)->clientResources(client());
        for (wl_resource *outputResource : resources) {
            d->send_leave(outputResource);
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
        for (wl_resource *outputResource : resources) {
            d->send_enter(outputResource);
        }
        d->outputDestroyedConnections[o] = connect(o, &Global::aboutToDestroyGlobal, this, [this, o] {
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
    return d->lockedPointer;
}

QPointer<ConfinedPointerInterface> SurfaceInterface::confinedPointer() const
{
    return d->confinedPointer;
}

bool SurfaceInterface::inhibitsIdle() const
{
    return !d->idleInhibitors.isEmpty();
}

void SurfaceInterface::setDataProxy(SurfaceInterface *surface)
{
    d->dataProxy = surface;
}

SurfaceInterface* SurfaceInterface::dataProxy() const
{
    return d->dataProxy;
}

QPointF SurfaceInterface::mapToBuffer(const QPointF &point) const
{
    return d->surfaceToBufferMatrix.map(point);
}

QPointF SurfaceInterface::mapFromBuffer(const QPointF &point) const
{
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
    return map_helper(d->surfaceToBufferMatrix, region);
}

QRegion SurfaceInterface::mapFromBuffer(const QRegion &region) const
{
    return map_helper(d->bufferToSurfaceMatrix, region);
}

QMatrix4x4 SurfaceInterface::surfaceToBufferMatrix() const
{
    return d->surfaceToBufferMatrix;
}

} // namespace KWaylandServer
