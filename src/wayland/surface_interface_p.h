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
#include <wayland-server-protocol.h>

namespace KWaylandServer
{
class IdleInhibitorV1Interface;
class SurfaceRole;
class ViewportInterface;

struct SurfaceState
{
    void mergeInto(SurfaceState *target);

    QRegion damage = QRegion();
    QRegion bufferDamage = QRegion();
    QRegion opaque = QRegion();
    QRegion input = infiniteRegion();
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
    KWin::Output::Transform bufferTransform = KWin::Output::Transform::Normal;
    wl_list frameCallbacks;
    QPoint offset = QPoint();
    QPointer<ClientBuffer> buffer;
    QPointer<ShadowInterface> shadow;
    QPointer<BlurInterface> blur;
    QPointer<ContrastInterface> contrast;
    QPointer<SlideInterface> slide;

    // Subsurfaces are stored in two lists. The below list contains subsurfaces that
    // are below their parent surface; the above list contains subsurfaces that are
    // placed above the parent surface.
    QList<SubSurfaceInterface *> below;
    QList<SubSurfaceInterface *> above;

    struct
    {
        QRectF sourceGeometry = QRectF();
        QSize destinationSize = QSize();
        bool sourceGeometryIsSet = false;
        bool destinationSizeIsSet = false;
    } viewport;
};

class SurfaceInterfacePrivate
{
public:
    static SurfaceInterfacePrivate *fromResource(wl_resource *resource);
    static SurfaceInterfacePrivate *get(SurfaceInterface *surface)
    {
        return surface->d.get();
    }

    explicit SurfaceInterfacePrivate(SurfaceInterface *q, wl_client *client, int version, uint32_t id);
    ~SurfaceInterfacePrivate();

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

    void commitToCache();
    void commitFromCache();

    void commitSubSurface();
    QMatrix4x4 buildSurfaceToBufferMatrix();
    void applyState(SurfaceState *next);

    bool computeEffectiveMapped() const;
    void updateEffectiveMapped();

    /**
     * Returns true if this surface (not including subsurfaces) contains a given point
     * @param position in surface-local co-ordiantes
     */
    bool contains(const QPointF &position) const;
    bool inputContains(const QPointF &position) const;

    CompositorInterface *compositor;
    SurfaceInterface *q;
    wl_resource *resource;
    SurfaceRole *role = nullptr;
    SurfaceState current;
    SurfaceState pending;
    SurfaceState cached;
    SubSurfaceInterface *subSurface = nullptr;
    QMatrix4x4 surfaceToBufferMatrix;
    QMatrix4x4 bufferToSurfaceMatrix;
    QSize bufferSize = QSize(0, 0);
    QSizeF implicitSurfaceSize = QSizeF(0, 0);
    QSizeF surfaceSize = QSizeF(0, 0);

    QRegion inputRegion;
    QRegion opaqueRegion;
    ClientBuffer *bufferRef = nullptr;
    bool mapped = false;
    bool hasCacheState = false;
    qreal scaleOverride = 1.;
    qreal pendingScaleOverride = 1.;

    QVector<OutputInterface *> outputs;

    LockedPointerV1Interface *lockedPointer = nullptr;
    ConfinedPointerV1Interface *confinedPointer = nullptr;
    QHash<OutputInterface *, QMetaObject::Connection> outputDestroyedConnections;
    QHash<OutputInterface *, QMetaObject::Connection> outputBoundConnections;

    QVector<IdleInhibitorV1Interface *> idleInhibitors;
    ViewportInterface *viewportExtension = nullptr;
    std::unique_ptr<LinuxDmaBufV1Feedback> dmabufFeedbackV1;
    ClientConnection *client = nullptr;

    static void surface_destroy_resource(wl_resource *resource);
    static void surface_destroy(wl_client *client, wl_resource *resource);
    static void surface_attach(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t x, int32_t y);
    static void surface_damage(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void surface_frame(wl_client *client, wl_resource *resource, uint32_t callback);
    static void surface_set_opaque_region(wl_client *client, wl_resource *resource, wl_resource *region);
    static void surface_set_input_region(wl_client *client, wl_resource *resource, wl_resource *region);
    static void surface_commit(wl_client *client, wl_resource *resource);
    static void surface_set_buffer_transform(wl_client *client, wl_resource *resource, int32_t transform);
    static void surface_set_buffer_scale(wl_client *client, wl_resource *resource, int32_t scale);
    static void surface_damage_buffer(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void surface_offset(wl_client *client, wl_resource *resource, int32_t x, int32_t y);

    static constexpr struct wl_surface_interface implementation = {
        .destroy = surface_destroy,
        .attach = surface_attach,
        .damage = surface_damage,
        .frame = surface_frame,
        .set_opaque_region = surface_set_opaque_region,
        .set_input_region = surface_set_input_region,
        .commit = surface_commit,
        .set_buffer_transform = surface_set_buffer_transform,
        .set_buffer_scale = surface_set_buffer_scale,
        .damage_buffer = surface_damage_buffer,
        .offset = surface_offset,
    };

private:
    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;
};

} // namespace KWaylandServer
