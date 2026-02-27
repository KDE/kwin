/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "xdgdbusannotation_v1.h"
#include "display.h"
#include "surface.h"

#include <QDebug>

namespace KWin
{

static constexpr uint32_t s_version = 1;

XdgDBusAnnotationManagerV1::XdgDBusAnnotationManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xdg_dbus_annotation_manager_v1(*display, s_version)
{
}

void XdgDBusAnnotationManagerV1::xdg_dbus_annotation_manager_v1_bind_resource(Resource *resource)
{
}

void XdgDBusAnnotationManagerV1::xdg_dbus_annotation_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}
void XdgDBusAnnotationManagerV1::xdg_dbus_annotation_manager_v1_annotate_client(Resource *resource, const QString &interface, uint32_t id)
{
    qWarning() << "annotate client" << resource << interface << id;

    // TODO
}
void XdgDBusAnnotationManagerV1::xdg_dbus_annotation_manager_v1_annotate_surface(Resource *resource, const QString &interface, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *annotation_resource = wl_resource_create(resource->client(), &xdg_dbus_annotation_v1_interface, resource->version(), id);
    if (!annotation_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto annot = new XdgDBusAnnotationV1(s, interface, annotation_resource);

    Q_EMIT annotationCreated(annot);
}

XdgDBusAnnotationV1::XdgDBusAnnotationV1(SurfaceInterface *surface, const QString &interface, wl_resource *resource)
    : QObject()
    , SurfaceExtension(surface)
    , QtWaylandServer::xdg_dbus_annotation_v1(resource)
    , m_surface(surface)
    , m_interface(interface)
{
}

XdgDBusAnnotationV1::~XdgDBusAnnotationV1()
{
}

void XdgDBusAnnotationV1::apply(XdgDBusAnnotationCommit *commit)
{
    if (commit->service.has_value()) {
        m_address = commit->service.value();
    }

    if (commit->objectPath.has_value()) {
        m_path = commit->objectPath.value();
    }

    Q_EMIT updated();
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_set_service(Resource *resource, const QString &service_name)
{
    pending->service = service_name;
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_set_object_path(Resource *resource, const QString &object_path)
{
    pending->objectPath = object_path;
}

SurfaceInterface *XdgDBusAnnotationV1::surface() const
{
    return m_surface;
}

QString XdgDBusAnnotationV1::service() const
{
    return m_address;
}

QString XdgDBusAnnotationV1::objectPath() const
{
    return m_path;
}

QString XdgDBusAnnotationV1::interface() const
{
    return m_interface;
}
}
