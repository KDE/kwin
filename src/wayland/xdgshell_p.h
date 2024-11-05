/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QIcon>

#include "qwayland-server-xdg-shell.h"
#include "xdgshell.h"

#include "surface_p.h"

namespace KWin
{
class XdgToplevelDecorationV1Interface;

class XdgShellInterfacePrivate : public QtWaylandServer::xdg_wm_base
{
public:
    XdgShellInterfacePrivate(XdgShellInterface *shell);

    Resource *resourceForXdgSurface(XdgSurfaceInterface *surface) const;

    void unregisterXdgSurface(XdgSurfaceInterface *surface);

    void registerPing(quint32 serial);

    static XdgShellInterfacePrivate *get(XdgShellInterface *shell);

    XdgShellInterface *q;
    Display *display = nullptr;
    QMap<quint32, QTimer *> pings;
    std::chrono::milliseconds pingTimeout = std::chrono::milliseconds(1000);

protected:
    void xdg_wm_base_destroy_resource(Resource *resource) override;
    void xdg_wm_base_destroy(Resource *resource) override;
    void xdg_wm_base_create_positioner(Resource *resource, uint32_t id) override;
    void xdg_wm_base_get_xdg_surface(Resource *resource, uint32_t id, ::wl_resource *surface) override;
    void xdg_wm_base_pong(Resource *resource, uint32_t serial) override;

private:
    QHash<XdgSurfaceInterface *, Resource *> xdgSurfaces;
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

struct XdgSurfaceCommit
{
    std::optional<QRect> windowGeometry;
    std::optional<quint32> acknowledgedConfigure;
};

struct XdgToplevelCommit : XdgSurfaceCommit
{
    std::optional<QSize> minimumSize;
    std::optional<QSize> maximumSize;
};

struct XdgPopupCommit : XdgSurfaceCommit
{
};

class XdgSurfaceInterfacePrivate : public QtWaylandServer::xdg_surface
{
public:
    XdgSurfaceInterfacePrivate(XdgSurfaceInterface *xdgSurface);

    void apply(XdgSurfaceCommit *commit);
    void reset();

    // These two point into XdgSurfaceRole's state and are valid as long as a role is assigned.
    XdgSurfaceCommit *pending = nullptr;

    XdgSurfaceInterface *q;
    XdgShellInterface *shell = nullptr;
    QPointer<XdgToplevelInterface> toplevel;
    QPointer<XdgPopupInterface> popup;
    QPointer<SurfaceInterface> surface;
    QRect windowGeometry;
    bool firstBufferAttached = false;
    bool isConfigured = false;
    bool isInitialized = false;

    static XdgSurfaceInterfacePrivate *get(XdgSurfaceInterface *surface);

protected:
    void xdg_surface_destroy_resource(Resource *resource) override;
    void xdg_surface_destroy(Resource *resource) override;
    void xdg_surface_get_toplevel(Resource *resource, uint32_t id) override;
    void xdg_surface_get_popup(Resource *resource, uint32_t id, ::wl_resource *parent, ::wl_resource *positioner) override;
    void xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void xdg_surface_ack_configure(Resource *resource, uint32_t serial) override;
};

class XdgToplevelInterfacePrivate : public SurfaceExtension<XdgToplevelCommit>, public QtWaylandServer::xdg_toplevel
{
public:
    XdgToplevelInterfacePrivate(XdgToplevelInterface *toplevel, XdgSurfaceInterface *surface);

    void apply(XdgToplevelCommit *commit) override;
    void reset();

    static XdgToplevelInterfacePrivate *get(XdgToplevelInterface *toplevel);
    static XdgToplevelInterfacePrivate *get(::wl_resource *resource);

    XdgToplevelInterface *q;
    QPointer<XdgToplevelInterface> parentXdgToplevel;
    QPointer<XdgToplevelDecorationV1Interface> decoration;
    XdgSurfaceInterface *xdgSurface;
    QString windowTitle;
    QString windowClass;
    QSize minimumSize = QSize(0, 0);
    QSize maximumSize = QSize(0, 0);
    QIcon customIcon; // managed externally by the xdg_toplevel_icon interface

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

class XdgPopupInterfacePrivate : public SurfaceExtension<XdgPopupCommit>, public QtWaylandServer::xdg_popup
{
public:
    static XdgPopupInterfacePrivate *get(XdgPopupInterface *popup);

    XdgPopupInterfacePrivate(XdgPopupInterface *popup, XdgSurfaceInterface *surface);

    void apply(XdgPopupCommit *commit) override;
    void reset();

    XdgPopupInterface *q;
    SurfaceInterface *parentSurface = nullptr;
    XdgSurfaceInterface *xdgSurface;
    XdgPositioner positioner;

protected:
    void xdg_popup_destroy_resource(Resource *resource) override;
    void xdg_popup_destroy(Resource *resource) override;
    void xdg_popup_grab(Resource *resource, ::wl_resource *seat, uint32_t serial) override;
    void xdg_popup_reposition(Resource *resource, struct ::wl_resource *positioner, uint32_t token) override;
};

} // namespace KWin
