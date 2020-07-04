/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgforeign_interface.h"
#include "xdgforeign_v2_interface_p.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

#include <QUuid>
#include <QDebug>

namespace KWaylandServer
{

class Q_DECL_HIDDEN XdgExporterUnstableV2Interface::Private : public Global::Private
{
public:
    Private(XdgExporterUnstableV2Interface *q, Display *d, XdgForeignInterface *foreignInterface);

    XdgForeignInterface *foreignInterface;
    QHash<QString, XdgExportedUnstableV2Interface *> exportedSurfaces;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void exportCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface);

    XdgExporterUnstableV2Interface *q;
    static const struct zxdg_exporter_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgExporterUnstableV2Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zxdg_exporter_v2_interface XdgExporterUnstableV2Interface::Private::s_interface = {
    destroyCallback,
    exportCallback
};
#endif

XdgExporterUnstableV2Interface::XdgExporterUnstableV2Interface(Display *display, XdgForeignInterface *parent)
    : Global(new Private(this, display, parent), parent)
{
}

XdgExporterUnstableV2Interface::~XdgExporterUnstableV2Interface()
{}

XdgExporterUnstableV2Interface::Private *XdgExporterUnstableV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

XdgExportedUnstableV2Interface *XdgExporterUnstableV2Interface::exportedSurface(const QString &handle)
{
    Q_D();

    auto it = d->exportedSurfaces.constFind(handle);
    if (it != d->exportedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

void XdgExporterUnstableV2Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void XdgExporterUnstableV2Interface::Private::exportCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * surface)
{
    auto s = cast(resource);
    QPointer <XdgExportedUnstableV2Interface> e = new XdgExportedUnstableV2Interface(s->q, surface);

    e->create(s->display->getConnection(client), wl_resource_get_version(resource), id);

    if (!e->resource()) {
        wl_resource_post_no_memory(resource);
        delete e;
        return;
    }

    const QString handle = QUuid::createUuid().toString();

    //a surface not exported anymore
    connect(e.data(), &XdgExportedUnstableV2Interface::unbound,
            s->q, [s, handle]() {
                s->exportedSurfaces.remove(handle);
                emit s->q->surfaceUnexported(handle);
            });

    //if the surface dies before this, this dies too
    connect(SurfaceInterface::get(surface), &QObject::destroyed,
            s->q, [s, e, handle]() {
                if (e) {
                    e->deleteLater();
                }
                s->exportedSurfaces.remove(handle);
                emit s->q->surfaceUnexported(handle);
            });

    s->exportedSurfaces[handle] = e;
    zxdg_exported_v2_send_handle(e->resource(), handle.toUtf8().constData());
    emit s->q->surfaceExported(handle, e);
}

XdgExporterUnstableV2Interface::Private::Private(XdgExporterUnstableV2Interface *q, Display *d,XdgForeignInterface *foreignInterface)
    : Global::Private(d, &zxdg_exporter_v2_interface, s_version)
    , foreignInterface(foreignInterface)
    , q(q)
{
}

void XdgExporterUnstableV2Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zxdg_exporter_v2_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void XdgExporterUnstableV2Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

class Q_DECL_HIDDEN XdgImporterUnstableV2Interface::Private : public Global::Private
{
public:
    Private(XdgImporterUnstableV2Interface *q, Display *d, XdgForeignInterface *foreignInterface);

    XdgForeignInterface *foreignInterface;

    QHash<QString, XdgImportedUnstableV2Interface *> importedSurfaces;

    //child->parent hash
    QHash<SurfaceInterface *, XdgImportedUnstableV2Interface *> parents;
    //parent->child hash
    QHash<XdgImportedUnstableV2Interface *, SurfaceInterface *> children;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void importCallback(wl_client *client, wl_resource *resource, uint32_t id, const char * handle);

    XdgImporterUnstableV2Interface *q;
    static const struct zxdg_importer_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgImporterUnstableV2Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zxdg_importer_v2_interface XdgImporterUnstableV2Interface::Private::s_interface = {
    destroyCallback,
    importCallback
};
#endif

XdgImporterUnstableV2Interface::XdgImporterUnstableV2Interface(Display *display, XdgForeignInterface *parent)
    : Global(new Private(this, display, parent), parent)
{
}

XdgImporterUnstableV2Interface::~XdgImporterUnstableV2Interface()
{
}

XdgImportedUnstableV2Interface *XdgImporterUnstableV2Interface::importedSurface(const QString &handle)
{
    Q_D();

    auto it = d->importedSurfaces.constFind(handle);
    if (it != d->importedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

SurfaceInterface *XdgImporterUnstableV2Interface::transientFor(SurfaceInterface *surface)
{
    Q_D();

    auto it = d->parents.constFind(surface);
    if (it == d->parents.constEnd()) {
        return nullptr;
    }
    return SurfaceInterface::get((*it)->parentResource());
}

XdgImporterUnstableV2Interface::Private *XdgImporterUnstableV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

void XdgImporterUnstableV2Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void XdgImporterUnstableV2Interface::Private::importCallback(wl_client *client, wl_resource *resource, uint32_t id, const char *h)
{
    auto s = cast(resource);

    Q_ASSERT(s->foreignInterface);
    const QString handle = QString::fromUtf8(h);

    XdgExportedUnstableV2Interface *exp = s->foreignInterface->d->exporter->exportedSurface(handle);
    if (!exp) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    wl_resource *surface = exp->parentResource();
    if (!surface) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    QPointer<XdgImportedUnstableV2Interface> imp = new XdgImportedUnstableV2Interface(s->q, surface);
    imp->create(s->display->getConnection(client), wl_resource_get_version(resource), id);

    //surface no longer exported
    connect(exp, &XdgExportedUnstableV2Interface::unbound,
            s->q, [s, imp, handle]() {
                //imp valid when the exported is deleted before the imported
                if (imp) {
                    zxdg_imported_v2_send_destroyed(imp->resource());
                    imp->deleteLater();
                }
                s->importedSurfaces.remove(handle);
                emit s->q->surfaceUnimported(handle);
            });

    connect(imp.data(), &XdgImportedUnstableV2Interface::childChanged,
            s->q, [s, imp](SurfaceInterface *child) {
                //remove any previous association
                auto it = s->children.find(imp);
                if (it != s->children.end()) {
                    s->parents.remove(*it);
                    s->children.erase(it);
                }

                s->parents[child] = imp;
                s->children[imp] = child;
                SurfaceInterface *parent = SurfaceInterface::get(imp->parentResource());
                emit s->q->transientChanged(child, parent);

                //child surface destroyed
                connect(child, &QObject::destroyed,
                        s->q, [s, child]() {
                            auto it = s->parents.find(child);
                            if (it != s->parents.end()) {
                                KWaylandServer::XdgImportedUnstableV2Interface* parent = *it;
                                s->children.remove(*it);
                                s->parents.erase(it);
                                emit s->q->transientChanged(nullptr, SurfaceInterface::get(parent->parentResource()));
                            }
                        });
            });

    //surface no longer imported
    connect(imp.data(), &XdgImportedUnstableV2Interface::unbound,
            s->q, [s, handle, imp]() {
                s->importedSurfaces.remove(handle);
                emit s->q->surfaceUnimported(handle);

                auto it = s->children.find(imp);
                if (it != s->children.end()) {
                    KWaylandServer::SurfaceInterface* child = *it;
                    s->parents.remove(*it);
                    s->children.erase(it);
                    emit s->q->transientChanged(child, nullptr);
                }
            });

    if (!imp->resource()) {
        wl_resource_post_no_memory(resource);
        delete imp;
        return;
    }

    s->importedSurfaces[handle] = imp;
    emit s->q->surfaceImported(handle, imp);
}

XdgImporterUnstableV2Interface::Private::Private(XdgImporterUnstableV2Interface *q, Display *d, XdgForeignInterface *foreignInterface)
    : Global::Private(d, &zxdg_importer_v2_interface, s_version)
    , foreignInterface(foreignInterface)
    , q(q)
{
}

void XdgImporterUnstableV2Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zxdg_importer_v2_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void XdgImporterUnstableV2Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

class Q_DECL_HIDDEN XdgExportedUnstableV2Interface::Private : public Resource::Private
{
public:
    Private(XdgExportedUnstableV2Interface *q, XdgExporterUnstableV2Interface *c, wl_resource *parentResource);
    ~Private();

private:

    XdgExportedUnstableV2Interface *q_func() {
        return reinterpret_cast<XdgExportedUnstableV2Interface *>(q);
    }

    static const struct zxdg_exported_v2_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zxdg_exported_v2_interface XdgExportedUnstableV2Interface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

XdgExportedUnstableV2Interface::XdgExportedUnstableV2Interface(XdgExporterUnstableV2Interface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

XdgExportedUnstableV2Interface::~XdgExportedUnstableV2Interface()
{}

XdgExportedUnstableV2Interface::Private *XdgExportedUnstableV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

XdgExportedUnstableV2Interface::Private::Private(XdgExportedUnstableV2Interface *q, XdgExporterUnstableV2Interface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &zxdg_exported_v2_interface, &s_interface)
{
}

XdgExportedUnstableV2Interface::Private::~Private()
{}

class Q_DECL_HIDDEN XdgImportedUnstableV2Interface::Private : public Resource::Private
{
public:
    Private(XdgImportedUnstableV2Interface *q, XdgImporterUnstableV2Interface *c, wl_resource *parentResource);
    ~Private();

    QPointer<SurfaceInterface> parentOf;

private:
    static void setParentOfCallback(wl_client *client, wl_resource *resource, wl_resource * surface);

    XdgImportedUnstableV2Interface *q_func() {
        return reinterpret_cast<XdgImportedUnstableV2Interface *>(q);
    }

    static const struct zxdg_imported_v2_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zxdg_imported_v2_interface XdgImportedUnstableV2Interface::Private::s_interface = {
    resourceDestroyedCallback,
    setParentOfCallback
};
#endif

XdgImportedUnstableV2Interface::XdgImportedUnstableV2Interface(XdgImporterUnstableV2Interface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

XdgImportedUnstableV2Interface::~XdgImportedUnstableV2Interface()
{}

XdgImportedUnstableV2Interface::Private *XdgImportedUnstableV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

SurfaceInterface *XdgImportedUnstableV2Interface::child() const
{
    Q_D();
    return d->parentOf.data();
}

void XdgImportedUnstableV2Interface::Private::setParentOfCallback(wl_client *client, wl_resource *resource, wl_resource * surface)
{
    Q_UNUSED(client)

    auto s = cast<Private>(resource);
    SurfaceInterface *surf = SurfaceInterface::get(surface);

    if (!surf) {
        return;
    }

    s->parentOf = surf;
    emit s->q_func()->childChanged(surf);
}

XdgImportedUnstableV2Interface::Private::Private(XdgImportedUnstableV2Interface *q, XdgImporterUnstableV2Interface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &zxdg_imported_v2_interface, &s_interface)
{
}

XdgImportedUnstableV2Interface::Private::~Private()
{}

}
