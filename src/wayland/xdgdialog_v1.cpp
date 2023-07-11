/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgdialog_v1.h"

#include "display.h"
#include "xdgshell.h"

#include "qwayland-server-dialog-v1.h"

#include <QHash>

namespace KWin
{

constexpr int version = 1;

class XdgDialogWmV1InterfacePrivate : public QtWaylandServer::xdg_wm_dialog_v1
{
public:
    XdgDialogWmV1InterfacePrivate(Display *display, XdgDialogWmV1Interface *q);

    XdgDialogWmV1Interface *const q;
    QHash<XdgToplevelInterface *, XdgDialogV1Interface *> m_dialogs;

protected:
    void xdg_wm_dialog_v1_get_xdg_dialog(Resource *resource, uint32_t id, wl_resource *toplevel) override;
    void xdg_wm_dialog_v1_destroy(Resource *resource) override;
};

class XdgDialogV1InterfacePrivate : public QtWaylandServer::xdg_dialog_v1
{
public:
    XdgDialogV1InterfacePrivate(wl_resource *resource, XdgDialogV1Interface *q);

    XdgDialogV1Interface *const q;
    XdgToplevelInterface *m_toplevel;
    bool modal = false;

protected:
    void xdg_dialog_v1_destroy(Resource *resource) override;
    void xdg_dialog_v1_destroy_resource(Resource *resource) override;
    void xdg_dialog_v1_set_modal(Resource *resource) override;
    void xdg_dialog_v1_unset_modal(Resource *resource) override;
};

XdgDialogWmV1InterfacePrivate::XdgDialogWmV1InterfacePrivate(Display *display, XdgDialogWmV1Interface *q)
    : xdg_wm_dialog_v1(*display, version)
    , q(q)
{
}

void XdgDialogWmV1InterfacePrivate::xdg_wm_dialog_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgDialogWmV1InterfacePrivate::xdg_wm_dialog_v1_get_xdg_dialog(Resource *resource, uint32_t id, wl_resource *toplevel_resource)
{
    auto toplevel = XdgToplevelInterface::get(toplevel_resource);
    if (!toplevel) {
        wl_resource_post_error(resource->handle, 0, "Invalid surface");
        return;
    }
    if (m_dialogs.value(toplevel)) {
        wl_resource_post_error(resource->handle, error::error_already_used, "xdg_toplevel already already used to a xdg_dialog_v1");
        return;
    }
    auto dialogResource = wl_resource_create(resource->client(), &xdg_dialog_v1_interface, resource->version(), id);
    auto dialog = new XdgDialogV1Interface(dialogResource, toplevel);
    m_dialogs.insert(toplevel, dialog);

    auto removeDialog = [this, toplevel, dialog] {
        m_dialogs.removeIf([toplevel, dialog](const std::pair<XdgToplevelInterface *, XdgDialogV1Interface *> &entry) {
            return entry.first == toplevel && entry.second == dialog;
        });
    };
    QObject::connect(dialog, &XdgDialogV1Interface::destroyed, q, removeDialog);
    QObject::connect(toplevel, &XdgDialogV1Interface::destroyed, q, removeDialog);

    Q_EMIT q->dialogCreated(dialog);
}

XdgDialogWmV1Interface::XdgDialogWmV1Interface(Display *display, QObject *parent)
    : d(std::make_unique<XdgDialogWmV1InterfacePrivate>(display, this))
{
}

XdgDialogWmV1Interface::~XdgDialogWmV1Interface() = default;

XdgDialogV1Interface *XdgDialogWmV1Interface::dialogForToplevel(XdgToplevelInterface *toplevel) const
{
    return d->m_dialogs.value(toplevel);
}

XdgDialogV1InterfacePrivate::XdgDialogV1InterfacePrivate(wl_resource *wl_resource, XdgDialogV1Interface *q)
    : xdg_dialog_v1(wl_resource)
    , q(q)
{
}

void XdgDialogV1InterfacePrivate::xdg_dialog_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgDialogV1InterfacePrivate::xdg_dialog_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgDialogV1InterfacePrivate::xdg_dialog_v1_set_modal(Resource *resource)
{
    if (modal) {
        return;
    }
    modal = true;
    Q_EMIT q->modalChanged(true);
}

void XdgDialogV1InterfacePrivate::xdg_dialog_v1_unset_modal(Resource *resource)
{
    if (!modal) {
        return;
    }
    modal = false;
    Q_EMIT q->modalChanged(false);
}

XdgDialogV1Interface::XdgDialogV1Interface(wl_resource *resource, XdgToplevelInterface *toplevel)
    : d(std::make_unique<XdgDialogV1InterfacePrivate>(resource, this))
{
    d->m_toplevel = toplevel;
}

XdgDialogV1Interface::~XdgDialogV1Interface()
{
}

bool XdgDialogV1Interface::isModal() const
{
    return d->modal;
}

XdgToplevelInterface *XdgDialogV1Interface::toplevel() const
{
    return d->m_toplevel;
}
}
