/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SURFACE_INTERFACE_P_H
#define WAYLAND_SERVER_SURFACE_INTERFACE_P_H

#include "surface_interface.h"
#include "resource_p.h"
// Qt
#include <QHash>
#include <QVector>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class IdleInhibitorInterface;
class SurfaceRole;

class SurfaceInterface::Private : public Resource::Private
{
public:
    struct State {
        QRegion damage = QRegion();
        QRegion bufferDamage = QRegion();
        QRegion opaque = QRegion();
        QRegion input = QRegion();
        bool inputIsSet = false;
        bool opaqueIsSet = false;
        bool bufferIsSet = false;
        bool shadowIsSet = false;
        bool blurIsSet = false;
        bool contrastIsSet = false;
        bool slideIsSet = false;
        bool inputIsInfinite = true;
        bool childrenChanged = false;
        bool scaleIsSet = false;
        bool transformIsSet = false;
        qint32 scale = 1;
        OutputInterface::Transform transform = OutputInterface::Transform::Normal;
        QList<wl_resource*> callbacks = QList<wl_resource*>();
        QPoint offset = QPoint();
        BufferInterface *buffer = nullptr;
        // stacking order: bottom (first) -> top (last)
        QList<QPointer<SubSurfaceInterface>> children;
        QPointer<ShadowInterface> shadow;
        QPointer<BlurInterface> blur;
        QPointer<ContrastInterface> contrast;
        QPointer<SlideInterface> slide;
    };
    Private(SurfaceInterface *q, CompositorInterface *c, wl_resource *parentResource);
    ~Private();

    void destroy();

    void addChild(QPointer<SubSurfaceInterface> subsurface);
    void removeChild(QPointer<SubSurfaceInterface> subsurface);
    bool raiseChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling);
    bool lowerChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling);
    void setShadow(const QPointer<ShadowInterface> &shadow);
    void setBlur(const QPointer<BlurInterface> &blur);
    void setContrast(const QPointer<ContrastInterface> &contrast);
    void setSlide(const QPointer<SlideInterface> &slide);
    void installPointerConstraint(LockedPointerInterface *lock);
    void installPointerConstraint(ConfinedPointerInterface *confinement);
    void installIdleInhibitor(IdleInhibitorInterface *inhibitor);

    void commitSubSurface();
    void commit();

    SurfaceRole *role = nullptr;

    State current;
    State pending;
    State subSurfacePending;
    QPointer<SubSurfaceInterface> subSurface;
    QRegion trackedDamage;

    // workaround for https://bugreports.qt.io/browse/QTBUG-52192
    // A subsurface needs to be considered mapped even if it doesn't have a buffer attached
    // Otherwise Qt's sub-surfaces will never be visible and the client will freeze due to
    // waiting on the frame callback of the never visible surface
    bool subSurfaceIsMapped = true;

    QVector<OutputInterface *> outputs;

    QPointer<LockedPointerInterface> lockedPointer;
    QPointer<ConfinedPointerInterface> confinedPointer;
    QHash<OutputInterface*, QMetaObject::Connection> outputDestroyedConnections;
    QVector<IdleInhibitorInterface*> idleInhibitors;

    SurfaceInterface *dataProxy = nullptr;

private:
    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;

    SurfaceInterface *q_func() {
        return reinterpret_cast<SurfaceInterface *>(q);
    }
    void swapStates(State *source, State *target, bool emitChanged);
    void damage(const QRect &rect);
    void damageBuffer(const QRect &rect);
    void setScale(qint32 scale);
    void setTransform(OutputInterface::Transform transform);
    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource *buffer, const QPoint &offset);
    void setOpaque(const QRegion &region);
    void setInput(const QRegion &region, bool isInfinite);

    static void destroyFrameCallback(wl_resource *r);

    static void attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy);
    static void damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void frameCallback(wl_client *client, wl_resource *resource, uint32_t callback);
    static void opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void commitCallback(wl_client *client, wl_resource *resource);
    // since version 2
    static void bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform);
    // since version 3
    static void bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale);
    // since version 4
    static void damageBufferCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);

    static const struct wl_surface_interface s_interface;
};

}
}

#endif
