/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgforeign_v2_interface_p.h"
#include "display.h"

#include <QUuid>

namespace KWaylandServer
{

static const quint32 s_exporterVersion = 1;
static const quint32 s_importerVersion = 1;

XdgForeignV2InterfacePrivate::XdgForeignV2InterfacePrivate(Display *display, XdgForeignV2Interface *_q)
    : q(_q)
    , exporter(new XdgExporterV2Interface(display, _q))
    , importer(new XdgImporterV2Interface(display, _q))
{
}

XdgForeignV2Interface::XdgForeignV2Interface(Display *display, QObject *parent)
    : QObject(parent),
      d(new XdgForeignV2InterfacePrivate(display, this))
{
}

XdgForeignV2Interface::~XdgForeignV2Interface()
{
}

SurfaceInterface *XdgForeignV2Interface::transientFor(SurfaceInterface *surface)
{
    return d->importer->transientFor(surface);
}

XdgExporterV2Interface::XdgExporterV2Interface(Display *display, XdgForeignV2Interface *foreign)
    : QObject(foreign)
    , QtWaylandServer::zxdg_exporter_v2(*display, s_exporterVersion)
    , m_foreign(foreign)
{
}

XdgExportedV2Interface *XdgExporterV2Interface::exportedSurface(const QString &handle)
{
    return m_exportedSurfaces.value(handle);
}

void XdgExporterV2Interface::zxdg_exporter_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgExporterV2Interface::zxdg_exporter_v2_export_toplevel(Resource *resource, uint32_t id,
                                                              wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *exportedResource = wl_resource_create(resource->client(),
                                                       &zxdg_exported_v2_interface,
                                                       resource->version(), id);
    if (!exportedResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    XdgExportedV2Interface *exported = new XdgExportedV2Interface(surface, exportedResource);
    const QString handle = QUuid::createUuid().toString();

    // a surface not exported anymore
    connect(exported, &XdgExportedV2Interface::destroyed, this, [this, handle]() {
        m_exportedSurfaces.remove(handle);
    });

    m_exportedSurfaces[handle] = exported;
    zxdg_exported_v2_send_handle(exportedResource, handle.toUtf8().constData());
}

XdgImporterV2Interface::XdgImporterV2Interface(Display *display, XdgForeignV2Interface *foreign)
    : QObject(foreign)
    , QtWaylandServer::zxdg_importer_v2(*display, s_importerVersion)
    , m_foreign(foreign)
{
}

XdgImportedV2Interface *XdgImporterV2Interface::importedSurface(const QString &handle)
{
    return m_importedSurfaces.value(handle);
}

SurfaceInterface *XdgImporterV2Interface::transientFor(SurfaceInterface *surface)
{
    auto it = m_parents.constFind(surface);
    if (it == m_parents.constEnd()) {
        return nullptr;
    }
    return (*it)->surface();
}

void XdgImporterV2Interface::zxdg_importer_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgImporterV2Interface::zxdg_importer_v2_import_toplevel(Resource *resource, uint32_t id, const QString &handle)
{
    wl_resource *importedResource = wl_resource_create(resource->client(),
                                                       &zxdg_imported_v2_interface,
                                                       resource->version(), id);
    if (!importedResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    // If there is no exported surface with the specified handle, we must still create an
    // inert xdg_imported object and send the destroyed event right afterwards.
    XdgExportedV2Interface *exported = m_foreign->d->exporter->exportedSurface(handle);
    if (!exported) {
        auto imported = new XdgDummyImportedV2Interface(importedResource);
        imported->send_destroyed();
        return;
    }

    XdgImportedV2Interface *imported = new XdgImportedV2Interface(exported, importedResource);

    connect(imported, &XdgImportedV2Interface::childChanged, this, [this, imported](SurfaceInterface *child) {
        // remove any previous association
        auto it = m_children.find(imported);
        if (it != m_children.end()) {
            m_parents.remove(*it);
            m_children.erase(it);
        }

        m_parents[child] = imported;
        m_children[imported] = child;
        SurfaceInterface *parent = imported->surface();
        emit m_foreign->transientChanged(child, parent);

        // child surface destroyed
        connect(child, &QObject::destroyed, this, [this, child]() {
            auto it = m_parents.find(child);
            if (it != m_parents.end()) {
                XdgImportedV2Interface *parent = *it;
                m_children.remove(*it);
                m_parents.erase(it);
                emit m_foreign->transientChanged(nullptr, parent->surface());
            }
        });
    });

    // surface no longer imported
    connect(imported, &XdgImportedV2Interface::destroyed, this, [this, handle, imported]() {
        m_importedSurfaces.remove(handle);

        auto it = m_children.find(imported);
        if (it != m_children.end()) {
            SurfaceInterface *child = *it;
            m_parents.remove(*it);
            m_children.erase(it);
            emit m_foreign->transientChanged(child, nullptr);
        }
    });

    m_importedSurfaces[handle] = imported;
}

XdgExportedV2Interface::XdgExportedV2Interface(SurfaceInterface *surface, wl_resource *resource )
    : QtWaylandServer::zxdg_exported_v2(resource)
    , m_surface(surface)
{
    connect(surface, &QObject::destroyed, this, &XdgExportedV2Interface::handleSurfaceDestroyed);
}

SurfaceInterface *XdgExportedV2Interface::surface()
{
    return m_surface;
}

void XdgExportedV2Interface::zxdg_exported_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgExportedV2Interface::zxdg_exported_v2_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void XdgExportedV2Interface::handleSurfaceDestroyed()
{
    delete this;
}

XdgDummyImportedV2Interface::XdgDummyImportedV2Interface(wl_resource *resource)
    : QtWaylandServer::zxdg_imported_v2(resource)
{
}

void XdgDummyImportedV2Interface::zxdg_imported_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgDummyImportedV2Interface::zxdg_imported_v2_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

XdgImportedV2Interface::XdgImportedV2Interface(XdgExportedV2Interface *exported, wl_resource *resource)
    : QtWaylandServer::zxdg_imported_v2(resource)
    , m_exported(exported)
{
    connect(exported, &QObject::destroyed, this, &XdgImportedV2Interface::handleExportedDestroyed);
}

SurfaceInterface *XdgImportedV2Interface::child() const
{
    return m_child;
}

SurfaceInterface *XdgImportedV2Interface::surface() const
{
    return m_exported->surface();
}

void XdgImportedV2Interface::zxdg_imported_v2_set_parent_of(Resource *resource, wl_resource *surface)
{
    Q_UNUSED(resource)
    SurfaceInterface *surf = SurfaceInterface::get(surface);

    if (!surf) {
        return;
    }

    m_child = surf;
    emit childChanged(surf);
}

void XdgImportedV2Interface::zxdg_imported_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgImportedV2Interface::zxdg_imported_v2_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void XdgImportedV2Interface::handleExportedDestroyed()
{
    send_destroyed();
    delete this;
}

}
