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
#include "surface_p.h"

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

void XdgDBusAnnotationManagerV1::xdg_dbus_annotation_manager_v1_annotate_surface(Resource *resource, const QString &interface, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, error_already_annotated, "Invalid surface");
        return;
    }

    wl_resource *annotation_resource = wl_resource_create(resource->client(), &xdg_dbus_annotation_v1_interface, resource->version(), id);
    if (!annotation_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto annot = new XdgDBusAnnotationV1(s, interface, annotation_resource);

    SurfaceInterfacePrivate::get(s)->installDBusAnnotation(annot);
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
    if (commit->bus.has_value()) {
        m_bus = commit->bus.value();
    }

    if (commit->busName.has_value()) {
        m_busName = commit->busName.value();
    }

    if (commit->objectPath.has_value()) {
        m_objectPath = commit->objectPath.value();
    }

    if (!m_bus.has_value()) {
        wl_resource_post_error(resource()->handle, error_incomplete, "Missing bus");
    }

    if (m_busName.isEmpty()) {
        wl_resource_post_error(resource()->handle, error_incomplete, "Missing bus name");
    }

    if (m_objectPath.isEmpty()) {
        wl_resource_post_error(resource()->handle, error_incomplete, "Missing object path");
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

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_set_bus(Resource *resource, uint32_t bus)
{
    pending->bus = static_cast<bus_enum>(bus);
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_set_bus_name(Resource *resource, const QString &bus_name)
{
    pending->busName = bus_name;
}

void XdgDBusAnnotationV1::xdg_dbus_annotation_v1_set_object_path(Resource *resource, const QString &object_path)
{
    pending->objectPath = object_path;
}

SurfaceInterface *XdgDBusAnnotationV1::surface() const
{
    return m_surface;
}

QtWaylandServer::xdg_dbus_annotation_v1::bus_enum XdgDBusAnnotationV1::bus() const
{
    return m_bus.value_or({});
}

QString XdgDBusAnnotationV1::service() const
{
    return m_busName;
}

QString XdgDBusAnnotationV1::objectPath() const
{
    return m_objectPath;
}

QString XdgDBusAnnotationV1::interface() const
{
    return m_interface;
}
}
