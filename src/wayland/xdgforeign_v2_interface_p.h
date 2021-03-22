/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "xdgforeign_v2_interface.h"
#include "surface_interface_p.h"

#include <qwayland-server-xdg-foreign-unstable-v2.h>

namespace KWaylandServer
{

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
    void transientChanged(KWaylandServer::SurfaceInterface *child, KWaylandServer::SurfaceInterface *parent);

private:
    QScopedPointer<XdgImporterV2InterfacePrivate> d;
};

class XdgExportedV2Interface : public QObject, QtWaylandServer::zxdg_exported_v2
{
    Q_OBJECT

public:
    explicit XdgExportedV2Interface(SurfaceInterface *surface, wl_resource *resource );

    SurfaceInterface *surface();

protected:
    void zxdg_exported_v2_destroy(Resource *resource) override;
    void zxdg_exported_v2_destroy_resource(Resource *resource) override;

private Q_SLOTS:
    void handleSurfaceDestroyed();

private:
    SurfaceInterface *m_surface;
};

class XdgImportedV2Interface : public QObject, QtWaylandServer::zxdg_imported_v2
{
    Q_OBJECT
public:
    explicit XdgImportedV2Interface(XdgExportedV2Interface *exported, wl_resource *resource);

    SurfaceInterface *child() const;
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void childChanged(KWaylandServer::SurfaceInterface *child);

private Q_SLOTS:
    void handleExportedDestroyed();

private:
    XdgExportedV2Interface *m_exported;
    QPointer<SurfaceInterface> m_child;

protected:
    void zxdg_imported_v2_set_parent_of(Resource *resource, wl_resource *surface) override;
    void zxdg_imported_v2_destroy(Resource *resource) override;
    void zxdg_imported_v2_destroy_resource(Resource *resource) override;
};
}
