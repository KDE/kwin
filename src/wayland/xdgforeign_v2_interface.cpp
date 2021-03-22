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
    : QObject(nullptr)
    , q(_q)
{
    exporter = new XdgExporterV2Interface(display, _q);
    importer = new XdgImporterV2Interface(display, _q);

    connect(importer, &XdgImporterV2Interface::transientChanged,
            q, &XdgForeignV2Interface::transientChanged);
}

XdgForeignV2Interface::XdgForeignV2Interface(Display *display, QObject *parent)
    : QObject(parent),
      d(new XdgForeignV2InterfacePrivate(display, this))
{
}

XdgForeignV2Interface::~XdgForeignV2Interface()
{
    delete d->exporter;
    delete d->importer;
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
    auto it = m_exportedSurfaces.constFind(handle);
    if (it != m_exportedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

void XdgExporterV2Interface::zxdg_exporter_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgExporterV2Interface::zxdg_exporter_v2_export_toplevel(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *XdgExported_resource = wl_resource_create(resource->client(), &zxdg_exported_v2_interface, resource->version(), id);
    if (!XdgExported_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }


    XdgExportedV2Interface * xdgExported = new XdgExportedV2Interface(s, XdgExported_resource);
    const QString handle = QUuid::createUuid().toString();

    //a surface not exported anymore
    QObject::connect(xdgExported, &XdgExportedV2Interface::destroyed,
            this, [this, handle]() {
                m_exportedSurfaces.remove(handle);
            });

    m_exportedSurfaces[handle] = xdgExported;
    zxdg_exported_v2_send_handle(XdgExported_resource, handle.toUtf8().constData());
}

XdgImporterV2Interface::XdgImporterV2Interface(Display *display, XdgForeignV2Interface *foreign)
    : QObject(foreign)
    , QtWaylandServer::zxdg_importer_v2(*display, s_importerVersion)
    , m_foreign(foreign)
{
}

XdgImportedV2Interface *XdgImporterV2Interface::importedSurface(const QString &handle)
{
    auto it = m_importedSurfaces.constFind(handle);
    if (it != m_importedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
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
    XdgExportedV2Interface *exp = m_foreign->d->exporter->exportedSurface(handle);
    if (!exp) {
        zxdg_imported_v2_send_destroyed(resource->handle);
        return;
    }

    wl_resource *XdgImported_resource = wl_resource_create(resource->client(), &zxdg_imported_v2_interface, resource->version(), id);
    if (!XdgImported_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    XdgImportedV2Interface * XdgImported = new XdgImportedV2Interface(exp, XdgImported_resource);

    QObject::connect(XdgImported, &XdgImportedV2Interface::childChanged,
            this, [this, XdgImported](SurfaceInterface *child) {
                //remove any previous association
                auto it = m_children.find(XdgImported);
                if (it != m_children.end()) {
                    m_parents.remove(*it);
                    m_children.erase(it);
                }

                m_parents[child] = XdgImported;
                m_children[XdgImported] = child;
                SurfaceInterface *parent = XdgImported->surface();
                emit transientChanged(child, parent);

                //child surface destroyed
                QObject::connect(child, &QObject::destroyed,
                        this, [this, child]() {
                            auto it = m_parents.find(child);
                            if (it != m_parents.end()) {
                                KWaylandServer::XdgImportedV2Interface* parent = *it;
                                m_children.remove(*it);
                                m_parents.erase(it);
                                emit transientChanged(nullptr, parent->surface());
                            }
                        });
            });

    //surface no longer imported
    QObject::connect(XdgImported, &XdgImportedV2Interface::destroyed,
            this, [this, handle, XdgImported]() {
                m_importedSurfaces.remove(handle);

                auto it = m_children.find(XdgImported);
                if (it != m_children.end()) {
                    KWaylandServer::SurfaceInterface* child = *it;
                    m_parents.remove(*it);
                    m_children.erase(it);
                    emit transientChanged(child, nullptr);
                }
            });

    m_importedSurfaces[handle] = XdgImported;
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
    send_destroyed(resource()->handle);
    delete this;
}

}
