/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H

#include "global.h"
#include "resource.h"

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class XdgExportedUnstableV2Interface;
class XdgImportedUnstableV2Interface;

class Q_DECL_HIDDEN XdgForeignInterface::Private
{
public:
    Private(Display *display, XdgForeignInterface *q);

    XdgForeignInterface *q;
    XdgExporterUnstableV2Interface *exporter;
    XdgImporterUnstableV2Interface *importer;
};

class Q_DECL_HIDDEN XdgExporterUnstableV2Interface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgExporterUnstableV2Interface();

    XdgExportedUnstableV2Interface *exportedSurface(const QString &handle);

Q_SIGNALS:
    void surfaceExported(const QString &handle, XdgExportedUnstableV2Interface *exported);
    void surfaceUnexported(const QString &handle);

private:
    explicit XdgExporterUnstableV2Interface(Display *display, XdgForeignInterface *parent = nullptr);
    friend class Display;
    friend class XdgForeignInterface;
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImporterUnstableV2Interface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgImporterUnstableV2Interface();

    XdgImportedUnstableV2Interface *importedSurface(const QString &handle);
    SurfaceInterface *transientFor(SurfaceInterface *surface);

Q_SIGNALS:
    void surfaceImported(const QString &handle, XdgImportedUnstableV2Interface *imported);
    void surfaceUnimported(const QString &handle);
    void transientChanged(KWayland::Server::SurfaceInterface *child, KWayland::Server::SurfaceInterface *parent);

private:
    explicit XdgImporterUnstableV2Interface(Display *display, XdgForeignInterface *parent = nullptr);
    friend class Display;
    friend class XdgForeignInterface;
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgExportedUnstableV2Interface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgExportedUnstableV2Interface();

private:
    explicit XdgExportedUnstableV2Interface(XdgExporterUnstableV2Interface *parent, wl_resource *parentResource);
    friend class XdgExporterUnstableV2Interface;

    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImportedUnstableV2Interface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgImportedUnstableV2Interface();

    SurfaceInterface *child() const;

Q_SIGNALS:
    void childChanged(KWayland::Server::SurfaceInterface *child);

private:
    explicit XdgImportedUnstableV2Interface(XdgImporterUnstableV2Interface *parent, wl_resource *parentResource);
    friend class XdgImporterUnstableV2Interface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
