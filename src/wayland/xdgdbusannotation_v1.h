/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kwin_export.h"
#include "qwayland-server-xdg-dbus-annotation-v1.h"

#include "surface.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace KWin
{

class Display;
class SurfaceInterface;
class XdgDBusAnnotationV1;

class KWIN_EXPORT XdgDBusAnnotationManagerV1 : public QObject, private QtWaylandServer::xdg_dbus_annotation_manager_v1
{
    Q_OBJECT
public:
    explicit XdgDBusAnnotationManagerV1(Display *display, QObject *parent);

Q_SIGNALS:
    void annotationCreated(KWin::XdgDBusAnnotationV1 *annotation);

private:
    void xdg_dbus_annotation_manager_v1_bind_resource(Resource *resource) override;

    void xdg_dbus_annotation_manager_v1_destroy(Resource *resource) override;
    void xdg_dbus_annotation_manager_v1_annotate_client(Resource *resource, const QString &interface, uint32_t id) override;
    void xdg_dbus_annotation_manager_v1_annotate_surface(Resource *resource, const QString &interface, uint32_t id, struct ::wl_resource *surface) override;
};

class XdgDBusAnnotationCommit : public SurfaceAttachedState<XdgDBusAnnotationCommit>
{
public:
    std::optional<QString> service;
    std::optional<QString> objectPath;
};

class XdgDBusAnnotationV1 : public QObject, public SurfaceExtension<XdgDBusAnnotationV1, XdgDBusAnnotationCommit>, private QtWaylandServer::xdg_dbus_annotation_v1
{
    Q_OBJECT
public:
    explicit XdgDBusAnnotationV1(const QString &interface, wl_resource *resource);
    explicit XdgDBusAnnotationV1(SurfaceInterface *surface, const QString &interface, wl_resource *resource);
    ~XdgDBusAnnotationV1() override;

    void apply(XdgDBusAnnotationCommit *commit);

    SurfaceInterface *surface() const;

    QString service() const;
    QString objectPath() const;
    QString interface() const;

Q_SIGNALS:
    void updated();

private:
    void xdg_dbus_annotation_v1_destroy_resource(Resource *resource) override;
    void xdg_dbus_annotation_v1_destroy(Resource *resource) override;
    void xdg_dbus_annotation_v1_set_service(Resource *resource, const QString &service_name) override;
    void xdg_dbus_annotation_v1_set_object_path(Resource *resource, const QString &object_path) override;

    QPointer<SurfaceInterface> m_surface;
    QString m_interface;
    QString m_address;
    QString m_path;
};

}
