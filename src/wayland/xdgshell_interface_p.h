/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-xdg-shell.h"
#include "xdgshell_interface.h"

#include "surface_interface.h"
#include "surfacerole_p.h"

namespace KWaylandServer
{
class XdgToplevelDecorationV1Interface;

class XdgShellInterfacePrivate : public QtWaylandServer::xdg_wm_base
{
public:
    XdgShellInterfacePrivate(XdgShellInterface *shell);

    Resource *resourceForXdgSurface(XdgSurfaceInterface *surface) const;

    void registerXdgSurface(XdgSurfaceInterface *surface);
    void unregisterXdgSurface(XdgSurfaceInterface *surface);

    void registerPing(quint32 serial);

    static XdgShellInterfacePrivate *get(XdgShellInterface *shell);

    XdgShellInterface *q;
    Display *display;
    QMap<quint32, QTimer *> pings;

protected:
    void xdg_wm_base_destroy(Resource *resource) override;
    void xdg_wm_base_create_positioner(Resource *resource, uint32_t id) override;
    void xdg_wm_base_get_xdg_surface(Resource *resource, uint32_t id, ::wl_resource *surface) override;
    void xdg_wm_base_pong(Resource *resource, uint32_t serial) override;

private:
    QMultiMap<wl_client *, XdgSurfaceInterface *> xdgSurfaces;
};

class XdgPositionerData : public QSharedData
{
public:
    Qt::Orientations slideConstraintAdjustments;
    Qt::Orientations flipConstraintAdjustments;
    Qt::Orientations resizeConstraintAdjustments;
    Qt::Edges anchorEdges;
    Qt::Edges gravityEdges;
    QPoint offset;
    QSize size;
    QRect anchorRect;
    bool isReactive = false;
    QSize parentSize;
    quint32 parentConfigure;
};

class XdgPositionerPrivate : public QtWaylandServer::xdg_positioner
{
public:
    XdgPositionerPrivate(::wl_resource *resource);

    QSharedDataPointer<XdgPositionerData> data;

    static XdgPositionerPrivate *get(::wl_resource *resource);

protected:
    void xdg_positioner_destroy_resource(Resource *resource) override;
    void xdg_positioner_destroy(Resource *resource) override;
    void xdg_positioner_set_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_positioner_set_anchor_rect(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_positioner_set_anchor(Resource *resource, uint32_t anchor) override;
    void xdg_positioner_set_gravity(Resource *resource, uint32_t gravity) override;
    void xdg_positioner_set_constraint_adjustment(Resource *resource, uint32_t constraint_adjustment) override;
    void xdg_positioner_set_offset(Resource *resource, int32_t x, int32_t y) override;
    void xdg_positioner_set_reactive(Resource *resource) override;
    void xdg_positioner_set_parent_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_positioner_set_parent_configure(Resource *resource, uint32_t serial) override;
};

class XdgSurfaceState
{
public:
    QRect windowGeometry;
    quint32 acknowledgedConfigure;
    bool acknowledgedConfigureIsSet = false;
    bool windowGeometryIsSet = false;
};

class XdgToplevelState
{
public:
    XdgSurfaceState base;
    QSize minimumSize;
    QSize maximumSize;
};

class XdgPopupState
{
public:
    XdgSurfaceState base;
};

template<typename State>
class XdgSurfaceRole : public SurfaceRole
{
public:
    XdgSurfaceRole(SurfaceInterface *surface, const QByteArray &name)
        : SurfaceRole(surface, name)
    {
    }

    State pending;
    State current;
};

class XdgSurfaceInterfacePrivate : public QtWaylandServer::xdg_surface
{
public:
    XdgSurfaceInterfacePrivate(XdgSurfaceInterface *xdgSurface);

    void applyState(XdgSurfaceState *next);
    void resetState();

    void unassignRole();
    void assignRole(XdgToplevelInterface *toplevel);
    void assignRole(XdgPopupInterface *popup);

    // These two point into XdgSurfaceRole's state and are valid as long as a role is assigned.
    XdgSurfaceState *current = nullptr;
    XdgSurfaceState *pending = nullptr;

    XdgSurfaceInterface *q;
    XdgShellInterface *shell;
    QPointer<XdgToplevelInterface> toplevel;
    QPointer<XdgPopupInterface> popup;
    QPointer<SurfaceInterface> surface;
    bool firstBufferAttached = false;
    bool isConfigured = false;

    static XdgSurfaceInterfacePrivate *get(XdgSurfaceInterface *surface);

protected:
    void xdg_surface_destroy_resource(Resource *resource) override;
    void xdg_surface_destroy(Resource *resource) override;
    void xdg_surface_get_toplevel(Resource *resource, uint32_t id) override;
    void xdg_surface_get_popup(Resource *resource, uint32_t id, ::wl_resource *parent, ::wl_resource *positioner) override;
    void xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_surface_ack_configure(Resource *resource, uint32_t serial) override;
};

class XdgToplevelInterfacePrivate : public XdgSurfaceRole<XdgToplevelState>, public QtWaylandServer::xdg_toplevel
{
public:
    XdgToplevelInterfacePrivate(XdgToplevelInterface *toplevel, XdgSurfaceInterface *surface);

    void commit() override;
    void reset();

    static XdgToplevelInterfacePrivate *get(XdgToplevelInterface *toplevel);
    static XdgToplevelInterfacePrivate *get(::wl_resource *resource);

    XdgToplevelInterface *q;
    QPointer<XdgToplevelInterface> parentXdgToplevel;
    QPointer<XdgToplevelDecorationV1Interface> decoration;
    QPointer<XdgSurfaceInterface> xdgSurface;
    QString windowTitle;
    QString windowClass;

protected:
    void xdg_toplevel_destroy_resource(Resource *resource) override;
    void xdg_toplevel_destroy(Resource *resource) override;
    void xdg_toplevel_set_parent(Resource *resource, ::wl_resource *parent) override;
    void xdg_toplevel_set_title(Resource *resource, const QString &title) override;
    void xdg_toplevel_set_app_id(Resource *resource, const QString &app_id) override;
    void xdg_toplevel_show_window_menu(Resource *resource, ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y) override;
    void xdg_toplevel_move(Resource *resource, ::wl_resource *seat, uint32_t serial) override;
    void xdg_toplevel_resize(Resource *resource, ::wl_resource *seat, uint32_t serial, uint32_t edges) override;
    void xdg_toplevel_set_max_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_toplevel_set_min_size(Resource *resource, int32_t width, int32_t height) override;
    void xdg_toplevel_set_maximized(Resource *resource) override;
    void xdg_toplevel_unset_maximized(Resource *resource) override;
    void xdg_toplevel_set_fullscreen(Resource *resource, ::wl_resource *output) override;
    void xdg_toplevel_unset_fullscreen(Resource *resource) override;
    void xdg_toplevel_set_minimized(Resource *resource) override;
};

class XdgPopupInterfacePrivate : public XdgSurfaceRole<XdgPopupState>, public QtWaylandServer::xdg_popup
{
public:
    static XdgPopupInterfacePrivate *get(XdgPopupInterface *popup);

    XdgPopupInterfacePrivate(XdgPopupInterface *popup, XdgSurfaceInterface *surface);

    void commit() override;
    void reset();

    XdgPopupInterface *q;
    SurfaceInterface *parentSurface;
    QPointer<XdgSurfaceInterface> xdgSurface;
    XdgPositioner positioner;

protected:
    void xdg_popup_destroy_resource(Resource *resource) override;
    void xdg_popup_destroy(Resource *resource) override;
    void xdg_popup_grab(Resource *resource, ::wl_resource *seat, uint32_t serial) override;
    void xdg_popup_reposition(Resource *resource, struct ::wl_resource *positioner, uint32_t token) override;
};

} // namespace KWaylandServer
