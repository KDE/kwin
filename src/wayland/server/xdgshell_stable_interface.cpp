/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgshell_stable_interface_p.h"
#include "xdgshell_interface_p.h"
#include "generic_shell_surface_p.h"
#include "display.h"
#include "global_p.h"
#include "global.h"
#include "resource_p.h"
#include "output_interface.h"
#include "seat_interface.h"
#include "surface_interface.h"

#include <wayland-xdg-shell-server-protocol.h>

namespace KWaylandServer
{

class XdgShellStableInterface::Private : public XdgShellInterface::Private
{
public:
    Private(XdgShellStableInterface *q, Display *d);

    QVector<XdgSurfaceStableInterface*> surfaces;
    QVector<XdgPositionerStableInterface*> positioners;

private:

    void createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource);
    void createPositioner(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource);

    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    quint32 ping(XdgShellSurfaceInterface * surface) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return static_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void createPositionerCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getXdgSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);
    static void pongCallback(wl_client *client, wl_resource *resource, uint32_t serial);

    XdgShellStableInterface *q;
    static const struct xdg_wm_base_interface s_interface;
    static const quint32 s_version;
    QHash<wl_client *, wl_resource*> resources;
};

class XdgPopupStableInterface::Private : public XdgShellPopupInterface::Private
{
public:
    Private(XdgPopupStableInterface *q, XdgShellStableInterface *c, SurfaceInterface *surface, wl_resource *parentResource);
    ~Private() override;

    QRect windowGeometry() const override;
    void commit() override;

    void ackConfigure(quint32 serial) {
        if (!configureSerials.contains(serial)) {
            return;
        }
        while (!configureSerials.isEmpty()) {
            quint32 i = configureSerials.takeFirst();
            emit q_func()->configureAcknowledged(i);
            if (i == serial) {
                break;
            }
        }
    }

    void popupDone() override;
    quint32 configure(const QRect &rect) override;

    XdgPopupStableInterface *q_func() {
        return static_cast<XdgPopupStableInterface *>(q);
    }
private:
    void setWindowGeometryCallback(const QRect &rect);

    static void grabCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial);

    static const struct xdg_popup_interface s_interface;

    struct ShellSurfaceState
    {
        QRect windowGeometry;

        bool windowGeometryIsSet = false;
    };

    ShellSurfaceState m_currentState;
    ShellSurfaceState m_pendingState;

    friend class XdgSurfaceStableInterface;
};

class XdgSurfaceStableInterface::Private : public KWaylandServer::Resource::Private
{
public:
    Private(XdgSurfaceStableInterface* q, XdgShellStableInterface* c, SurfaceInterface* surface, wl_resource* parentResource);

    ~Private() override;

    XdgSurfaceStableInterface *q_func() {
        return static_cast<XdgSurfaceStableInterface *>(q);
    }

    void createTopLevel(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource);
    void createPopup(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource, wl_resource *parentWindow, wl_resource *positioner);
    XdgShellStableInterface *m_shell;
    SurfaceInterface *m_surface;

    //effectively a union, only one of these should be populated.
    //a surface cannot have two roles
    QPointer<XdgTopLevelStableInterface> m_topLevel;
    QPointer<XdgPopupStableInterface> m_popup;

private:
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getTopLevelCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getPopupCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *parent, wl_resource *positioner);
    static void ackConfigureCallback(wl_client *client, wl_resource *resource, uint32_t serial);
    static void setWindowGeometryCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static const struct xdg_surface_interface s_interface;
};

class XdgTopLevelStableInterface::Private : public XdgShellSurfaceInterface::Private
{
public:
    Private(XdgTopLevelStableInterface* q, XdgShellStableInterface* c, SurfaceInterface* surface, wl_resource* parentResource);
    ~Private() override;

    QRect windowGeometry() const override;
    QSize minimumSize() const override;
    QSize maximumSize() const override;
    void close() override;
    void commit() override;

    void ackConfigure(quint32 serial) {
        if (!configureSerials.contains(serial)) {
            return;
        }
        while (!configureSerials.isEmpty()) {
            quint32 i = configureSerials.takeFirst();
            emit q_func()->configureAcknowledged(i);
            if (i == serial) {
                break;
            }
        }
    }

    quint32 configure(States states, const QSize &size) override {
        if (!resource) {
            return 0;
        }
        const quint32 serial = global->display()->nextSerial();
        wl_array configureStates;
        wl_array_init(&configureStates);
        if (states.testFlag(State::Maximized)) {
            uint32_t *s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
            *s = XDG_TOPLEVEL_STATE_MAXIMIZED;
        }
        if (states.testFlag(State::Fullscreen)) {
            uint32_t *s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
            *s = XDG_TOPLEVEL_STATE_FULLSCREEN;
        }
        if (states.testFlag(State::Resizing)) {
            uint32_t *s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
            *s = XDG_TOPLEVEL_STATE_RESIZING;
        }
        if (states.testFlag(State::Activated)) {
            uint32_t *s = static_cast<uint32_t*>(wl_array_add(&configureStates, sizeof(uint32_t)));
            *s = XDG_TOPLEVEL_STATE_ACTIVATED;
        }
        configureSerials << serial;
        xdg_toplevel_send_configure(resource, size.width(), size.height(), &configureStates);

        xdg_surface_send_configure(parentResource, serial);

        client->flush();
        wl_array_release(&configureStates);
        return serial;
    };

    XdgTopLevelStableInterface *q_func() {
        return static_cast<XdgTopLevelStableInterface*>(q);
    }

private:
    void setWindowGeometryCallback(const QRect &rect);

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void setParentCallback(struct wl_client *client, struct wl_resource *resource, wl_resource *parent);
    static void showWindowMenuCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial, int32_t x, int32_t y);
    static void setMaxSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height);
    static void setMinSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height);
    static void setMaximizedCallback(wl_client *client, wl_resource *resource);
    static void unsetMaximizedCallback(wl_client *client, wl_resource *resource);
    static void setFullscreenCallback(wl_client *client, wl_resource *resource, wl_resource *output);
    static void unsetFullscreenCallback(wl_client *client, wl_resource *resource);
    static void setMinimizedCallback(wl_client *client, wl_resource *resource);

    static const struct xdg_toplevel_interface s_interface;

    struct ShellSurfaceState
    {
        QRect windowGeometry;
        QSize minimumSize = QSize(0, 0);
        QSize maximiumSize = QSize(INT32_MAX, INT32_MAX);

        bool windowGeometryIsSet = false;
        bool minimumSizeIsSet = false;
        bool maximumSizeIsSet = false;
    };

    ShellSurfaceState m_currentState;
    ShellSurfaceState m_pendingState;

    friend class XdgSurfaceStableInterface;
};


const quint32 XdgShellStableInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct xdg_wm_base_interface XdgShellStableInterface::Private::s_interface = {
    destroyCallback,
    createPositionerCallback,
    getXdgSurfaceCallback,
    pongCallback
};
#endif

void XdgShellStableInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    auto s = cast(resource);
    if (!s->surfaces.isEmpty()) {
        wl_resource_post_error(resource, XDG_WM_BASE_ERROR_DEFUNCT_SURFACES, "WMBase destroyed before surfaces");
    }

    wl_resource_destroy(resource);
}

void XdgShellStableInterface::Private::createPositionerCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    auto s = cast(resource);
    s->createPositioner(client, wl_resource_get_version(resource), id, resource);
}

void XdgShellStableInterface::Private::getXdgSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    auto s = cast(resource);
    s->createSurface(client, wl_resource_get_version(resource), id, SurfaceInterface::get(surface), resource);
}

void XdgShellStableInterface::Private::createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource)
{
    auto it = std::find_if(surfaces.constBegin(), surfaces.constEnd(),
        [surface](XdgSurfaceStableInterface *s) {
            return surface == s->surface();
        }
    );
    if (it != surfaces.constEnd()) {
        wl_resource_post_error(surface->resource(), XDG_WM_BASE_ERROR_ROLE, "XDG Surface already created");
        return;
    }
    XdgSurfaceStableInterface *shellSurface = new XdgSurfaceStableInterface(q, surface, parentResource);
    surfaces << shellSurface;
    QObject::connect(shellSurface, &XdgSurfaceStableInterface::destroyed, q,
        [this, shellSurface] {
            surfaces.removeAll(shellSurface);
        }
    );

    shellSurface->d->create(display->getConnection(client), version, id);
}

void XdgShellStableInterface::Private::createPositioner(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource)
{
    Q_UNUSED(client)

    XdgPositionerStableInterface *positioner = new XdgPositionerStableInterface(q, parentResource);
    positioners << positioner;
    QObject::connect(positioner, &Resource::destroyed, q,
        [this, positioner] {
            positioners.removeAll(positioner);
        }
    );
    positioner->d->create(display->getConnection(client), version, id);
}

void XdgShellStableInterface::Private::pongCallback(wl_client *client, wl_resource *resource, uint32_t serial)
{
    Q_UNUSED(client)
    auto s = cast(resource);
    auto timerIt = s->pingTimers.find(serial);
    if (timerIt != s->pingTimers.end() && timerIt.value()->isActive()) {
        delete timerIt.value();
        s->pingTimers.erase(timerIt);
        emit s->q->pongReceived(serial);
    }
}

XdgShellStableInterface::Private::Private(XdgShellStableInterface *q, Display *d)
    : XdgShellInterface::Private(XdgShellInterfaceVersion::Stable, q, d, &xdg_wm_base_interface, 1)
    , q(q)
{
}

void XdgShellStableInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    auto resource = c->createResource(&xdg_wm_base_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    resources[client] = resource;
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void XdgShellStableInterface::Private::unbind(wl_resource *resource)
{
    auto s = cast(resource);
    auto client = wl_resource_get_client(resource);
    s->resources.remove(client);
}

XdgTopLevelStableInterface *XdgShellStableInterface::getSurface(wl_resource *resource)
{
    if (!resource) {
        return nullptr;
    }
    Q_D();

    for (auto it = d->surfaces.constBegin(); it != d->surfaces.constEnd() ; it++) {
        auto topLevel = (*it)->topLevel();
        if (topLevel && topLevel->resource() == resource) {
            return topLevel;
        }
    }
    return nullptr;
}

XdgSurfaceStableInterface *XdgShellStableInterface::realGetSurface(wl_resource *resource)
{
    if (!resource) {
        return nullptr;
    }
    Q_D();

    for (auto it = d->surfaces.constBegin(); it != d->surfaces.constEnd() ; it++) {
        if ((*it)->resource() == resource) {
            return (*it);
        }
    }
    return nullptr;
}

XdgPositionerStableInterface *XdgShellStableInterface::getPositioner(wl_resource *resource)
{
    if (!resource) {
        return nullptr;
    }
    Q_D();
    for (auto it = d->positioners.constBegin(); it != d->positioners.constEnd() ; it++) {
        if ((*it)->resource() == resource) {
            return *it;
        }
    }
    return nullptr;
}

quint32 XdgShellStableInterface::Private::ping(XdgShellSurfaceInterface *surface)
{
    auto client = surface->client()->client();
    //from here we can get the resource bound to our global.

    auto clientXdgShellResource = resources.value(client);
    if (!clientXdgShellResource) {
        return 0;
    }

    const quint32 pingSerial = display->nextSerial();
    xdg_wm_base_send_ping(clientXdgShellResource, pingSerial);

    setupTimer(pingSerial);
    return pingSerial;
}

XdgShellStableInterface::Private *XdgShellStableInterface::d_func() const
{
    return static_cast<Private*>(d.data());
}

namespace {
template <>
Qt::Edges edgesToQtEdges(xdg_toplevel_resize_edge edges)
{
    Qt::Edges qtEdges;
    switch (edges) {
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_TOPLEVEL_RESIZE_EDGE_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    return qtEdges;
}
}

#ifndef K_DOXYGEN
const struct xdg_surface_interface XdgSurfaceStableInterface::Private::s_interface = {
    destroyCallback,
    getTopLevelCallback,
    getPopupCallback,
    setWindowGeometryCallback,
    ackConfigureCallback
};
#endif

void XdgSurfaceStableInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void XdgSurfaceStableInterface::Private::getTopLevelCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    auto s = cast<XdgSurfaceStableInterface::Private>(resource);
    s->createTopLevel(client, wl_resource_get_version(resource), id, resource);
}

void XdgSurfaceStableInterface::Private::createTopLevel(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource)
{
    // FIXME: That's incorrect! The client may have asked us to create an xdg-toplevel
    // for a pointer surface or a subsurface. We have to post an error in that case.
    if (m_topLevel) {
        wl_resource_post_error(parentResource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "Toplevel already created on this surface");
        return;
    }
    if (m_popup) {
        wl_resource_post_error(parentResource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "Popup already created on this surface");
        return;
    }
    m_topLevel = new XdgTopLevelStableInterface (m_shell, m_surface, parentResource);
    m_topLevel->d->create(m_shell->display()->getConnection(client), version, id);

    emit m_shell->surfaceCreated(m_topLevel);
}


void XdgSurfaceStableInterface::Private::getPopupCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *parent, wl_resource *positioner)
{
    auto s = cast<XdgSurfaceStableInterface::Private>(resource);
    s->createPopup(client, wl_resource_get_version(resource), id, resource, parent, positioner);
}

void XdgSurfaceStableInterface::Private::createPopup(wl_client *client, uint32_t version, uint32_t id, wl_resource *parentResource, wl_resource *parentSurface, wl_resource *positioner)
{
    // FIXME: That's incorrect! The client may have asked us to create an xdg-popup
    // for a pointer surface or a subsurface. We have to post an error in that case.
    if (m_topLevel) {
        wl_resource_post_error(parentResource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "Toplevel already created on this surface");
        return;
    }
    if (m_popup) {
        wl_resource_post_error(parentResource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "Popup already created on this surface");
        return;
    }

    auto xdgPositioner = m_shell->getPositioner(positioner);
    if (!xdgPositioner) {
        wl_resource_post_error(parentResource, XDG_WM_BASE_ERROR_INVALID_POSITIONER, "Invalid positioner");
        return;
    }
    m_popup = new XdgPopupStableInterface(m_shell, m_surface, parentResource);
    auto pd = m_popup->d_func();

    pd->create(m_shell->display()->getConnection(client), version, id);

    auto parentXdgSurface = m_shell->realGetSurface(parentSurface);
    if (!parentXdgSurface) {
        wl_resource_post_error(parentResource, XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT, "Invalid popup parent");
        return;
    }
    pd->parent = parentXdgSurface->surface();
    pd->initialSize = xdgPositioner->initialSize();
    pd->anchorRect = xdgPositioner->anchorRect();
    pd->anchorEdge = xdgPositioner->anchorEdge();
    pd->gravity = xdgPositioner->gravity();
    pd->constraintAdjustments = xdgPositioner->constraintAdjustments();
    pd->anchorOffset = xdgPositioner->anchorOffset();

    emit m_shell->xdgPopupCreated(m_popup.data());
}


void XdgSurfaceStableInterface::Private::ackConfigureCallback(wl_client *client, wl_resource *resource, uint32_t serial)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);

    if (s->m_topLevel) {
        s->m_topLevel->d_func()->ackConfigure(serial);
    } else if (s->m_popup) {
        s->m_popup->d_func()->ackConfigure(serial);
    }
}

void XdgSurfaceStableInterface::Private::setWindowGeometryCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);

    if (width < 0 || height < 0) {
        wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE , "Tried to set invalid xdg-surface geometry");
        return;
    }

    if (s->m_topLevel) {
        s->m_topLevel->d_func()->setWindowGeometryCallback(QRect(x, y, width, height));
    } else if (s->m_popup) {
        s->m_popup->d_func()->setWindowGeometryCallback(QRect(x, y, width, height));
    }
}

XdgSurfaceStableInterface::Private::Private(XdgSurfaceStableInterface *q, XdgShellStableInterface *c, SurfaceInterface *surface, wl_resource *parentResource)
    : KWaylandServer::Resource::Private(q, c, parentResource, &xdg_surface_interface, &s_interface),
    m_shell(c),
    m_surface(surface)
{
}

XdgSurfaceStableInterface::Private::~Private() = default;


class XdgPositionerStableInterface::Private : public KWaylandServer::Resource::Private
{
public:
    Private(XdgPositionerStableInterface *q,  XdgShellStableInterface *c, wl_resource* parentResource);

    QSize initialSize;
    QRect anchorRect;
    Qt::Edges anchorEdge;
    Qt::Edges gravity;
    PositionerConstraints constraintAdjustments;
    QPoint anchorOffset;

private:
    static void setSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height);
    static void setAnchorRectCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void setAnchorCallback(wl_client *client, wl_resource *resource, uint32_t anchor);
    static void setGravityCallback(wl_client *client, wl_resource *resource, uint32_t gravity);
    static void setConstraintAdjustmentCallback(wl_client *client, wl_resource *resource, uint32_t constraint_adjustment);
    static void setOffsetCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y);

    static const struct xdg_positioner_interface s_interface;
};

XdgPositionerStableInterface::Private::Private(XdgPositionerStableInterface *q, XdgShellStableInterface *c, wl_resource *parentResource)
    : KWaylandServer::Resource::Private(q, c, parentResource, &xdg_positioner_interface, &s_interface)
{
}

#ifndef K_DOXYGEN
const struct xdg_positioner_interface XdgPositionerStableInterface::Private::s_interface = {
    resourceDestroyedCallback,
    setSizeCallback,
    setAnchorRectCallback,
    setAnchorCallback,
    setGravityCallback,
    setConstraintAdjustmentCallback,
    setOffsetCallback
};
#endif


void XdgPositionerStableInterface::Private::setSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height) {
    Q_UNUSED(client)
    auto s = cast<Private>(resource);
    s->initialSize = QSize(width, height);
}

void XdgPositionerStableInterface::Private::setAnchorRectCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    auto s = cast<Private>(resource);
    s->anchorRect = QRect(x, y, width, height);
}

void XdgPositionerStableInterface::Private::setAnchorCallback(wl_client *client, wl_resource *resource, uint32_t anchor) {
    Q_UNUSED(client)

    auto s = cast<Private>(resource);

    Qt::Edges qtEdges;
    switch (anchor) {
    case XDG_POSITIONER_ANCHOR_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_POSITIONER_ANCHOR_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_ANCHOR_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_ANCHOR_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    s->anchorEdge = qtEdges;
}

void XdgPositionerStableInterface::Private::setGravityCallback(wl_client *client, wl_resource *resource, uint32_t gravity) {
    Q_UNUSED(client)
    auto s = cast<Private>(resource);

    Qt::Edges qtEdges;
    switch (gravity) {
    case XDG_POSITIONER_GRAVITY_TOP:
        qtEdges = Qt::TopEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM:
        qtEdges = Qt::BottomEdge;
        break;
    case XDG_POSITIONER_GRAVITY_LEFT:
        qtEdges = Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
        qtEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
        qtEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case XDG_POSITIONER_GRAVITY_RIGHT:
        qtEdges = Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
        qtEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
        qtEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case XDG_POSITIONER_GRAVITY_NONE:
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    s->gravity = qtEdges;
}

void XdgPositionerStableInterface::Private::setConstraintAdjustmentCallback(wl_client *client, wl_resource *resource, uint32_t constraint_adjustment) {
    Q_UNUSED(client)
    auto s = cast<Private>(resource);
    PositionerConstraints constraints;
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X) {
        constraints |= PositionerConstraint::SlideX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y) {
        constraints |= PositionerConstraint::SlideY;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        constraints |= PositionerConstraint::FlipX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        constraints |= PositionerConstraint::FlipY;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X) {
        constraints |= PositionerConstraint::ResizeX;
    }
    if (constraint_adjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y) {
        constraints |= PositionerConstraint::ResizeY;
    }
    s->constraintAdjustments = constraints;
}

void XdgPositionerStableInterface::Private::setOffsetCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(client)
    auto s = cast<Private>(resource);
    s->anchorOffset = QPoint(x,y);
}

QRect XdgTopLevelStableInterface::Private::windowGeometry() const
{
    return m_currentState.windowGeometry;
}

QSize XdgTopLevelStableInterface::Private::minimumSize() const
{
    return m_currentState.minimumSize;
}

QSize XdgTopLevelStableInterface::Private::maximumSize() const
{
    return m_currentState.maximiumSize;
}

void XdgTopLevelStableInterface::Private::close()
{
    xdg_toplevel_send_close(resource);
    client->flush();
}

void XdgTopLevelStableInterface::Private::commit()
{
    const bool windowGeometryChanged = m_pendingState.windowGeometryIsSet;
    const bool minimumSizeChanged = m_pendingState.minimumSizeIsSet;
    const bool maximumSizeChanged = m_pendingState.maximumSizeIsSet;

    if (windowGeometryChanged) {
        m_currentState.windowGeometry = m_pendingState.windowGeometry;
    }
    if (minimumSizeChanged) {
        m_currentState.minimumSize = m_pendingState.minimumSize;
    }
    if (maximumSizeChanged) {
        m_currentState.maximiumSize = m_pendingState.maximiumSize;
    }

    m_pendingState = ShellSurfaceState{};

    if (windowGeometryChanged) {
        emit q_func()->windowGeometryChanged(m_currentState.windowGeometry);
    }
    if (minimumSizeChanged) {
        emit q_func()->minSizeChanged(m_currentState.minimumSize);
    }
    if (maximumSizeChanged) {
        emit q_func()->maxSizeChanged(m_currentState.maximiumSize);
    }
}

void XdgTopLevelStableInterface::Private::setMaxSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE, "Tried to set invalid xdg-toplevel maximum size");
        return;
    }
    if (width == 0) {
        width = INT32_MAX;
    }
    if (height == 0) {
        height = INT32_MAX;
    }
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->m_pendingState.maximiumSize = QSize(width, height);
    s->m_pendingState.maximumSizeIsSet = true;
}

void XdgTopLevelStableInterface::Private::setMinSizeCallback(wl_client *client, wl_resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE, "Tried to set invalid xdg-toplevel minimum size");
        return;
    }
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->m_pendingState.minimumSize = QSize(width, height);
    s->m_pendingState.minimumSizeIsSet = true;
}

void XdgTopLevelStableInterface::Private::setWindowGeometryCallback(const QRect &rect)
{
    m_pendingState.windowGeometry = rect;
    m_pendingState.windowGeometryIsSet = true;
}

const struct xdg_toplevel_interface XdgTopLevelStableInterface::Private::s_interface = {
    destroyCallback,
    setParentCallback,
    setTitleCallback,
    setAppIdCallback,
    showWindowMenuCallback,
    moveCallback,
    resizeCallback<xdg_toplevel_resize_edge>,
    setMaxSizeCallback,
    setMinSizeCallback,
    setMaximizedCallback,
    unsetMaximizedCallback,
    setFullscreenCallback,
    unsetFullscreenCallback,
    setMinimizedCallback
};

void XdgTopLevelStableInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void XdgTopLevelStableInterface::Private::setParentCallback(wl_client *client, wl_resource *resource, wl_resource *parent)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    if (!parent) {
        //setting null is valid API. Clear
        s->parent = nullptr;
        emit s->q_func()->transientForChanged();
    } else {
        auto parentSurface = static_cast<XdgShellStableInterface*>(s->q->global())->getSurface(parent);
        if (s->parent.data() != parentSurface) {
            s->parent = QPointer<XdgTopLevelStableInterface>(parentSurface);
            emit s->q_func()->transientForChanged();
        }
    }
}

void XdgTopLevelStableInterface::Private::showWindowMenuCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    emit s->q_func()->windowMenuRequested(SeatInterface::get(seat), serial, QPoint(x, y));
}

XdgTopLevelStableInterface::Private::Private(XdgTopLevelStableInterface *q, XdgShellStableInterface *c, SurfaceInterface *surface, wl_resource *parentResource)
    : XdgShellSurfaceInterface::Private(XdgShellInterfaceVersion::Stable, q, c, surface, parentResource, &xdg_toplevel_interface, &s_interface)
{
}

void XdgTopLevelStableInterface::Private::setMaximizedCallback(wl_client *client, wl_resource *resource)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->q_func()->maximizedChanged(true);
}

void XdgTopLevelStableInterface::Private::unsetMaximizedCallback(wl_client *client, wl_resource *resource)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->q_func()->maximizedChanged(false);
}

void XdgTopLevelStableInterface::Private::setFullscreenCallback(wl_client *client, wl_resource *resource, wl_resource *output)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    OutputInterface *o = nullptr;
    if (output) {
        o = OutputInterface::get(output);
    }
    s->q_func()->fullscreenChanged(true, o);
}

void XdgTopLevelStableInterface::Private::unsetFullscreenCallback(wl_client *client, wl_resource *resource)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->q_func()->fullscreenChanged(false, nullptr);
}

void XdgTopLevelStableInterface::Private::setMinimizedCallback(wl_client *client, wl_resource *resource)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->q_func()->minimizeRequested();
}

XdgTopLevelStableInterface::Private::~Private() = default;

#ifndef K_DOXYGEN
const struct xdg_popup_interface XdgPopupStableInterface::Private::s_interface = {
    resourceDestroyedCallback,
    grabCallback
};
#endif

XdgPopupStableInterface::Private::Private(XdgPopupStableInterface *q, XdgShellStableInterface *c, SurfaceInterface *surface, wl_resource *parentResource)
    : XdgShellPopupInterface::Private(XdgShellInterfaceVersion::Stable, q, c, surface, parentResource, &xdg_popup_interface, &s_interface)
{
}

QRect XdgPopupStableInterface::Private::windowGeometry() const
{
    return m_currentState.windowGeometry;
}

void XdgPopupStableInterface::Private::commit()
{
    const bool windowGeometryChanged = m_pendingState.windowGeometryIsSet;

    if (windowGeometryChanged) {
        m_currentState.windowGeometry = m_pendingState.windowGeometry;
    }

    m_pendingState = ShellSurfaceState{};

    if (windowGeometryChanged) {
        emit q_func()->windowGeometryChanged(m_currentState.windowGeometry);
    }
}

void XdgPopupStableInterface::Private::setWindowGeometryCallback(const QRect &rect)
{
    m_pendingState.windowGeometry = rect;
    m_pendingState.windowGeometryIsSet = true;
}

void XdgPopupStableInterface::Private::grabCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial)
{
    Q_UNUSED(client)
    auto s = cast<Private>(resource);
    auto seatInterface = SeatInterface::get(seat);
    s->q_func()->grabRequested(seatInterface, serial);
}

XdgPopupStableInterface::Private::~Private() = default;

quint32 XdgPopupStableInterface::Private::configure(const QRect &rect)
{
    if (!resource) {
        return 0;
    }
    const quint32 serial = global->display()->nextSerial();
    configureSerials << serial;
    xdg_popup_send_configure(resource, rect.x(), rect.y(), rect.width(), rect.height());
    xdg_surface_send_configure(parentResource, serial);
    client->flush();

    return serial;
}

void XdgPopupStableInterface::Private::popupDone()
{
    if (!resource) {
        return;
    }
    // TODO: dismiss all child popups
    xdg_popup_send_popup_done(resource);
    client->flush();
}

XdgShellStableInterface::XdgShellStableInterface(Display *display, QObject *parent)
    : XdgShellInterface(new Private(this, display), parent)
{
}

Display* XdgShellStableInterface::display() const
{
    return d->display;
}

XdgShellStableInterface::~XdgShellStableInterface() = default;

XdgSurfaceStableInterface::XdgSurfaceStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource)
    : KWaylandServer::Resource(new Private(this, parent, surface, parentResource))
{
}

XdgSurfaceStableInterface::~XdgSurfaceStableInterface() = default;

SurfaceInterface* XdgSurfaceStableInterface::surface() const
{
    Q_D();
    return d->m_surface;
}

XdgPositionerStableInterface::XdgPositionerStableInterface(XdgShellStableInterface *parent, wl_resource *parentResource)
    : KWaylandServer::Resource(new Private(this, parent, parentResource))
{
}

QSize XdgPositionerStableInterface::initialSize() const
{
    Q_D();
    return d->initialSize;
}

QRect XdgPositionerStableInterface::anchorRect() const
{
    Q_D();
    return d->anchorRect;
}

Qt::Edges XdgPositionerStableInterface::anchorEdge() const
{
    Q_D();
    return d->anchorEdge;
}

Qt::Edges XdgPositionerStableInterface::gravity() const
{
    Q_D();
    return d->gravity;
}

PositionerConstraints XdgPositionerStableInterface::constraintAdjustments() const
{
    Q_D();
    return d->constraintAdjustments;
}

QPoint XdgPositionerStableInterface::anchorOffset() const
{
    Q_D();
    return d->anchorOffset;
}


XdgPositionerStableInterface::Private *XdgPositionerStableInterface::d_func() const
{
    return static_cast<Private*>(d.data());
}


XdgTopLevelStableInterface* XdgSurfaceStableInterface::topLevel() const
{
    Q_D();
    return d->m_topLevel.data();
}

XdgPopupStableInterface* XdgSurfaceStableInterface::popup() const
{
    Q_D();
    return d->m_popup.data();
}

XdgSurfaceStableInterface::Private *XdgSurfaceStableInterface::d_func() const
{
    return static_cast<Private*>(d.data());
}


XdgTopLevelStableInterface::XdgTopLevelStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource)
    : KWaylandServer::XdgShellSurfaceInterface(new Private(this, parent, surface, parentResource))
{
}

XdgTopLevelStableInterface::~XdgTopLevelStableInterface() = default;

XdgTopLevelStableInterface::Private *XdgTopLevelStableInterface::d_func() const
{
    return static_cast<Private*>(d.data());
}

XdgPopupStableInterface::XdgPopupStableInterface(XdgShellStableInterface *parent, SurfaceInterface *surface, wl_resource *parentResource)
    : XdgShellPopupInterface(new Private(this, parent, surface, parentResource))
{
}

XdgPopupStableInterface::~XdgPopupStableInterface() = default;

XdgPopupStableInterface::Private *XdgPopupStableInterface::d_func() const
{
    return static_cast<Private*>(d.data());
}

}

