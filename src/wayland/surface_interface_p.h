/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "surface_interface.h"
#include "utils.h"
// Qt
#include <QHash>
#include <QVector>
// Wayland
#include "qwayland-server-wayland.h"

namespace KWaylandServer
{

class IdleInhibitorV1Interface;
class SurfaceRole;
class ViewportInterface;

class KWaylandFrameCallback : public QtWaylandServer::wl_callback
{
public:
    KWaylandFrameCallback(wl_resource *resource, SurfaceInterface *surface);

    void destroy();

    QPointer<SurfaceInterface> surface;

protected:
    void callback_destroy_resource(Resource *resource) override;
};

struct SurfaceState
{
    QRegion damage = QRegion();
    QRegion bufferDamage = QRegion();
    QRegion opaque = QRegion();
    QRegion input = infiniteRegion();
    QSize size = QSize();
    bool inputIsSet = false;
    bool opaqueIsSet = false;
    bool bufferIsSet = false;
    bool shadowIsSet = false;
    bool blurIsSet = false;
    bool contrastIsSet = false;
    bool slideIsSet = false;
    bool childrenChanged = false;
    bool bufferScaleIsSet = false;
    bool bufferTransformIsSet = false;
    qint32 bufferScale = 1;
    OutputInterface::Transform bufferTransform = OutputInterface::Transform::Normal;
    QList<KWaylandFrameCallback *> frameCallbacks;
    QPoint offset = QPoint();
    BufferInterface *buffer = nullptr;
    QPointer<ShadowInterface> shadow;
    QPointer<BlurInterface> blur;
    QPointer<ContrastInterface> contrast;
    QPointer<SlideInterface> slide;

    // Subsurfaces are stored in two lists. The below list contains subsurfaces that
    // are below their parent surface; the above list contains subsurfaces that are
    // placed above the parent surface.
    QList<SubSurfaceInterface *> below;
    QList<SubSurfaceInterface *> above;

    struct {
        QRectF sourceGeometry = QRectF();
        QSize destinationSize = QSize();
        bool sourceGeometryIsSet = false;
        bool destinationSizeIsSet = false;
    } viewport;
};

class SurfaceInterfacePrivate : public QtWaylandServer::wl_surface
{
public:
    static SurfaceInterfacePrivate *get(SurfaceInterface *surface) { return surface->d.data(); }

    explicit SurfaceInterfacePrivate(SurfaceInterface *q);
    ~SurfaceInterfacePrivate() override;

    void addChild(SubSurfaceInterface *subsurface);
    void removeChild(SubSurfaceInterface *subsurface);
    bool raiseChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor);
    bool lowerChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor);
    void setShadow(const QPointer<ShadowInterface> &shadow);
    void setBlur(const QPointer<BlurInterface> &blur);
    void setContrast(const QPointer<ContrastInterface> &contrast);
    void setSlide(const QPointer<SlideInterface> &slide);
    void installPointerConstraint(LockedPointerV1Interface *lock);
    void installPointerConstraint(ConfinedPointerV1Interface *confinement);
    void installIdleInhibitor(IdleInhibitorV1Interface *inhibitor);

    void commit();
    QMatrix4x4 buildSurfaceToBufferMatrix(const SurfaceState *state);
    void swapStates(SurfaceState *source, SurfaceState *target, bool emitChanged);

    CompositorInterface *compositor;
    SurfaceInterface *q;
    SurfaceRole *role = nullptr;
    SurfaceState current;
    SurfaceState pending;
    SurfaceState cached;
    SubSurfaceInterface *subSurface = nullptr;
    QMatrix4x4 surfaceToBufferMatrix;
    QMatrix4x4 bufferToSurfaceMatrix;
    QSize bufferSize;
    QRegion inputRegion;

    // workaround for https://bugreports.qt.io/browse/QTBUG-52192
    // A subsurface needs to be considered mapped even if it doesn't have a buffer attached
    // Otherwise Qt's sub-surfaces will never be visible and the client will freeze due to
    // waiting on the frame callback of the never visible surface
    bool subSurfaceIsMapped = true;

    QVector<OutputInterface *> outputs;

    LockedPointerV1Interface *lockedPointer = nullptr;
    ConfinedPointerV1Interface *confinedPointer = nullptr;
    QHash<OutputInterface*, QMetaObject::Connection> outputDestroyedConnections;
    QHash<OutputInterface*, QMetaObject::Connection> outputBoundConnections;

    QVector<IdleInhibitorV1Interface*> idleInhibitors;
    ViewportInterface *viewportExtension = nullptr;
    SurfaceInterface *dataProxy = nullptr;

    static QList<SurfaceInterface *> surfaces;

    ClientConnection *client = nullptr;

protected:
    void surface_destroy_resource(Resource *resource) override;
    void surface_destroy(Resource *resource) override;
    void surface_attach(Resource *resource, struct ::wl_resource *buffer, int32_t x, int32_t y) override;
    void surface_damage(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void surface_frame(Resource *resource, uint32_t callback) override;
    void surface_set_opaque_region(Resource *resource, struct ::wl_resource *region) override;
    void surface_set_input_region(Resource *resource, struct ::wl_resource *region) override;
    void surface_commit(Resource *resource) override;
    void surface_set_buffer_transform(Resource *resource, int32_t transform) override;
    void surface_set_buffer_scale(Resource *resource, int32_t scale) override;
    void surface_damage_buffer(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;

private:
    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;
};

} // namespace KWaylandServer
