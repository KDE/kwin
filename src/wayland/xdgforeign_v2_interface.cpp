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

class XdgExporterV2InterfacePrivate : public QtWaylandServer::zxdg_exporter_v2
{
public:
    XdgExporterV2InterfacePrivate(XdgExporterV2Interface *_q, Display *display, XdgForeignV2Interface *foreignInterface);

    XdgForeignV2Interface *foreignInterface;
    QHash<QString, XdgExportedV2Interface *> exportedSurfaces;
    XdgExporterV2Interface *q;

protected:
    void zxdg_exporter_v2_destroy(Resource *resource) override;
    void zxdg_exporter_v2_export_toplevel(Resource *resource, uint32_t id, wl_resource *surface) override;

};

XdgExporterV2Interface::XdgExporterV2Interface(Display *display, XdgForeignV2Interface *parent)
    : QObject(parent)
    , d(new XdgExporterV2InterfacePrivate(this, display, parent))
{
}

XdgExporterV2Interface::~XdgExporterV2Interface() = default;

XdgExportedV2Interface *XdgExporterV2Interface::exportedSurface(const QString &handle)
{
    auto it = d->exportedSurfaces.constFind(handle);
    if (it != d->exportedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

void XdgExporterV2InterfacePrivate::zxdg_exporter_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgExporterV2InterfacePrivate::zxdg_exporter_v2_export_toplevel(Resource *resource, uint32_t id, wl_resource *surface)
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
            q, [this, handle]() {
                exportedSurfaces.remove(handle);
                emit q->surfaceUnexported(handle);
            });

    //if the surface dies before this, this dies too
    QObject::connect(SurfaceInterface::get(surface), &QObject::destroyed,
            q, [this, xdgExported, handle]() {
                if (xdgExported) {
                    xdgExported->deleteLater();
                }
                exportedSurfaces.remove(handle);
                emit q->surfaceUnexported(handle);
            });

    exportedSurfaces[handle] = xdgExported;
    zxdg_exported_v2_send_handle(XdgExported_resource, handle.toUtf8().constData());
    emit q->surfaceExported(handle, xdgExported);
}

XdgExporterV2InterfacePrivate::XdgExporterV2InterfacePrivate(XdgExporterV2Interface *_q, Display *display, XdgForeignV2Interface *foreignInterface)
    : QtWaylandServer::zxdg_exporter_v2(*display, s_exporterVersion)
    , foreignInterface(foreignInterface)
    , q(_q)
{
}

class XdgImporterV2InterfacePrivate : public QtWaylandServer::zxdg_importer_v2
{
public:
    XdgImporterV2InterfacePrivate(XdgImporterV2Interface *_q, Display *display, XdgForeignV2Interface *foreignInterface);

    XdgForeignV2Interface *foreignInterface;
    QHash<QString, XdgImportedV2Interface *> importedSurfaces;

    //child->parent hash
    QHash<SurfaceInterface *, XdgImportedV2Interface *> parents;
    //parent->child hash
    QHash<XdgImportedV2Interface *, SurfaceInterface *> children;
    XdgImporterV2Interface *q;

protected:
    void zxdg_importer_v2_destroy(Resource *resource) override;
    void zxdg_importer_v2_import_toplevel(Resource *resource, uint32_t id, const QString &handle) override;
};

XdgImporterV2Interface::XdgImporterV2Interface(Display *display, XdgForeignV2Interface *parent)
    : QObject(parent)
    , d(new XdgImporterV2InterfacePrivate(this, display, parent))
{
}

XdgImporterV2Interface::~XdgImporterV2Interface() = default;

XdgImportedV2Interface *XdgImporterV2Interface::importedSurface(const QString &handle)
{
    auto it = d->importedSurfaces.constFind(handle);
    if (it != d->importedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

SurfaceInterface *XdgImporterV2Interface::transientFor(SurfaceInterface *surface)
{
    auto it = d->parents.constFind(surface);
    if (it == d->parents.constEnd()) {
        return nullptr;
    }
    return (*it)->parentResource();
}

void XdgImporterV2InterfacePrivate::zxdg_importer_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgImporterV2InterfacePrivate::zxdg_importer_v2_import_toplevel(Resource *resource, uint32_t id, const QString &handle)
{
    Q_ASSERT(foreignInterface);

    XdgExportedV2Interface *exp = foreignInterface->d->exporter->exportedSurface(handle);
    if (!exp) {
        zxdg_imported_v2_send_destroyed(resource->handle);
        return;
    }

    SurfaceInterface *surface = exp->parentResource();
    if (!surface) {
        zxdg_imported_v2_send_destroyed(resource->handle);
        return;
    }

    wl_resource *XdgImported_resource = wl_resource_create(resource->client(), &zxdg_imported_v2_interface, resource->version(), id);
    if (!XdgImported_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    XdgImportedV2Interface * XdgImported = new XdgImportedV2Interface(surface, XdgImported_resource);

    //surface no longer exported
    QObject::connect(exp, &XdgExportedV2Interface::destroyed,
            q, [this, XdgImported, handle, XdgImported_resource]() {
                //imp valid when the exported is deleted before the imported
                if (XdgImported) {
                    zxdg_imported_v2_send_destroyed(XdgImported_resource);
                    XdgImported->deleteLater();
                }
                importedSurfaces.remove(handle);
                emit q->surfaceUnimported(handle);
            });

    QObject::connect(XdgImported, &XdgImportedV2Interface::childChanged,
            q, [this, XdgImported](SurfaceInterface *child) {
                //remove any previous association
                auto it = children.find(XdgImported);
                if (it != children.end()) {
                    parents.remove(*it);
                    children.erase(it);
                }

                parents[child] = XdgImported;
                children[XdgImported] = child;
                SurfaceInterface *parent = XdgImported->parentResource();
                emit q->transientChanged(child, parent);

                //child surface destroyed
                QObject::connect(child, &QObject::destroyed,
                        q, [this, child]() {
                            auto it = parents.find(child);
                            if (it != parents.end()) {
                                KWaylandServer::XdgImportedV2Interface* parent = *it;
                                children.remove(*it);
                                parents.erase(it);
                                emit q->transientChanged(nullptr, parent->parentResource());
                            }
                        });
            });

    //surface no longer imported
    QObject::connect(XdgImported, &XdgImportedV2Interface::destroyed,
            q, [this, handle, XdgImported]() {
                importedSurfaces.remove(handle);
                emit q->surfaceUnimported(handle);

                auto it = children.find(XdgImported);
                if (it != children.end()) {
                    KWaylandServer::SurfaceInterface* child = *it;
                    parents.remove(*it);
                    children.erase(it);
                    emit q->transientChanged(child, nullptr);
                }
            });

    importedSurfaces[handle] = XdgImported;
    emit q->surfaceImported(handle, XdgImported);
}

XdgImporterV2InterfacePrivate::XdgImporterV2InterfacePrivate(XdgImporterV2Interface *_q, Display *display, XdgForeignV2Interface *foreignInterface)
    : QtWaylandServer::zxdg_importer_v2(*display, s_importerVersion)
    , foreignInterface(foreignInterface)
    , q(_q)

{
}

XdgExportedV2Interface::XdgExportedV2Interface(SurfaceInterface *surface, wl_resource *resource )
    : QObject(nullptr)
    , QtWaylandServer::zxdg_exported_v2(resource)
    , surface(surface)
{
}

XdgExportedV2Interface::~XdgExportedV2Interface() = default;

SurfaceInterface *XdgExportedV2Interface::parentResource()
{
    return surface;
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

XdgImportedV2Interface::XdgImportedV2Interface(SurfaceInterface *surface, wl_resource *resource)
    : QObject(nullptr)
    , QtWaylandServer::zxdg_imported_v2(resource)
    , surface(surface)
{
}

XdgImportedV2Interface::~XdgImportedV2Interface() = default;

SurfaceInterface *XdgImportedV2Interface::child() const
{
    return parentOf.data();
}

SurfaceInterface *XdgImportedV2Interface::parentResource()
{
    return surface;
}

void XdgImportedV2Interface::zxdg_imported_v2_set_parent_of(Resource *resource, wl_resource *surface)
{
    Q_UNUSED(resource)
    SurfaceInterface *surf = SurfaceInterface::get(surface);

    if (!surf) {
        return;
    }

    parentOf = surf;
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
}
