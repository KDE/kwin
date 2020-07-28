/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H

#include "xdgforeign_v2_interface.h"
#include "surface_interface_p.h"

#include <qwayland-server-xdg-foreign-unstable-v2.h>

namespace KWaylandServer
{

class Display;
class SurfaceInterface;
class XdgExportedV2Interface;
class XdgImportedV2Interface;
class XdgExporterV2InterfacePrivate;
class XdgImporterV2InterfacePrivate;
class XdgExportedV2Interface;
class XdgImportedV2InterfacePrivate;

class XdgForeignV2InterfacePrivate : public QObject
{
    Q_OBJECT
public:
    XdgForeignV2InterfacePrivate(Display *display, XdgForeignV2Interface *q);

    XdgForeignV2Interface *q;
    XdgExporterV2Interface *exporter;
    XdgImporterV2Interface *importer;
};

class XdgExporterV2Interface : public QObject
{
    Q_OBJECT
public:
    explicit XdgExporterV2Interface(Display *display, XdgForeignV2Interface *parent = nullptr);
    ~XdgExporterV2Interface() override;

    XdgExportedV2Interface *exportedSurface(const QString &handle);

Q_SIGNALS:
    void surfaceExported(const QString &handle, XdgExportedV2Interface *exported);
    void surfaceUnexported(const QString &handle);

private:
    QScopedPointer<XdgExporterV2InterfacePrivate> d;
};

class XdgImporterV2Interface : public QObject
{
    Q_OBJECT
public:
    explicit XdgImporterV2Interface(Display *display, XdgForeignV2Interface *parent = nullptr);
    ~XdgImporterV2Interface() override;

    XdgImportedV2Interface *importedSurface(const QString &handle);
    SurfaceInterface *transientFor(SurfaceInterface *surface);

Q_SIGNALS:
    void surfaceImported(const QString &handle, XdgImportedV2Interface *imported);
    void surfaceUnimported(const QString &handle);
    void transientChanged(KWaylandServer::SurfaceInterface *child, KWaylandServer::SurfaceInterface *parent);

private:
    QScopedPointer<XdgImporterV2InterfacePrivate> d;
};

class XdgExportedV2Interface : public QObject, QtWaylandServer::zxdg_exported_v2
{
public:
    explicit XdgExportedV2Interface(SurfaceInterface *surface, wl_resource *resource );
    ~XdgExportedV2Interface() override;

    SurfaceInterface *parentResource();

private:
    SurfaceInterface *surface;

protected:
    void zxdg_exported_v2_destroy(Resource *resource) override;
    void zxdg_exported_v2_destroy_resource(Resource *resource) override;

};

class XdgImportedV2Interface : public QObject, QtWaylandServer::zxdg_imported_v2
{
    Q_OBJECT
public:
    explicit XdgImportedV2Interface(SurfaceInterface *surface, wl_resource *resource);
    ~XdgImportedV2Interface() override;

    SurfaceInterface *child() const;
    SurfaceInterface *parentResource();

Q_SIGNALS:
    void childChanged(KWaylandServer::SurfaceInterface *child);

private:
    SurfaceInterface *surface;
    QPointer<SurfaceInterface> parentOf;

protected:
    void zxdg_imported_v2_set_parent_of(Resource *resource, wl_resource *surface) override;
    void zxdg_imported_v2_destroy(Resource *resource) override;
    void zxdg_imported_v2_destroy_resource(Resource *resource) override;

};
}

#endif
