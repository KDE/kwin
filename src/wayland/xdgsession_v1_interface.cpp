/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgsession_v1_interface.h"

#include "display.h"
#include "xdgshell_interface_p.h"

#include "qwayland-server-xdg-session-management-v1.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

class XdgSessionManagerV1InterfacePrivate : public QtWaylandServer::xdg_session_manager_v1
{
public:
    XdgSessionManagerV1InterfacePrivate(Display *display, XdgSessionStorageV1 *storage, XdgSessionManagerV1Interface *q);

    XdgSessionManagerV1Interface *q;
    QHash<QString, XdgApplicationSessionV1Interface *> sessions;
    XdgSessionStorageV1 *storage;

protected:
    void xdg_session_manager_v1_destroy(Resource *resource) override;
    void xdg_session_manager_v1_get_session(Resource *resource, uint32_t id, const QString &session) override;
};

XdgSessionManagerV1InterfacePrivate::XdgSessionManagerV1InterfacePrivate(Display *display,
                                                                         XdgSessionStorageV1 *storage,
                                                                         XdgSessionManagerV1Interface *q)
    : QtWaylandServer::xdg_session_manager_v1(*display, s_version)
    , q(q)
    , storage(storage)
{
}

void XdgSessionManagerV1InterfacePrivate::xdg_session_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgSessionManagerV1InterfacePrivate::xdg_session_manager_v1_get_session(Resource *resource, uint32_t id, const QString &session)
{
    QString sessionHandle = session;
    if (sessionHandle.isEmpty()) {
        sessionHandle = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (sessions.contains(sessionHandle)) {
        wl_resource_post_error(resource->handle, error_in_use,
                               "%s session is already in use", qPrintable(sessionHandle));
        return;
    }

    wl_resource *sessionResource = wl_resource_create(resource->client(),
                                                      &xdg_session_v1_interface,
                                                      resource->version(), id);
    if (!sessionResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto applicationSession = new XdgApplicationSessionV1Interface(storage, sessionHandle, sessionResource);
    sessions.insert(sessionHandle, applicationSession);
    QObject::connect(applicationSession, &QObject::destroyed, q, [this, sessionHandle]() {
        sessions.remove(sessionHandle);
    });
}

XdgSessionManagerV1Interface::XdgSessionManagerV1Interface(Display *display, XdgSessionStorageV1 *storage, QObject *parent)
    : QObject(parent)
    , d(new XdgSessionManagerV1InterfacePrivate(display, storage, this))
{
}

XdgSessionManagerV1Interface::~XdgSessionManagerV1Interface()
{
}

XdgSessionStorageV1 *XdgSessionManagerV1Interface::storage() const
{
    return d->storage;
}

class XdgApplicationSessionV1InterfacePrivate : public QtWaylandServer::xdg_session_v1
{
public:
    XdgApplicationSessionV1InterfacePrivate(XdgSessionStorageV1 *storage, const QString &sessionId, wl_resource *resource, XdgApplicationSessionV1Interface *q);

    XdgApplicationSessionV1Interface *q;
    XdgSessionStorageV1 *storage;
    QHash<QString, XdgToplevelSessionV1Interface *> sessions;
    QString sessionId;

protected:
    void xdg_session_v1_destroy_resource(Resource *resource) override;
    void xdg_session_v1_destroy(Resource *resource) override;
    void xdg_session_v1_remove(Resource *resource) override;
    void xdg_session_v1_get_toplevel_session(Resource *resource, uint32_t id, struct ::wl_resource *toplevel, const QString &toplevel_id) override;
    void xdg_session_v1_remove_toplevel_session(Resource *resource, const QString &toplevel_id) override;
};

XdgApplicationSessionV1InterfacePrivate::XdgApplicationSessionV1InterfacePrivate(XdgSessionStorageV1 *storage,
                                                                                 const QString &sessionId,
                                                                                 wl_resource *resource,
                                                                                 XdgApplicationSessionV1Interface *q)
    : QtWaylandServer::xdg_session_v1(resource)
    , q(q)
    , storage(storage)
    , sessionId(sessionId)
{
    if (storage->contains(sessionId)) {
        send_restored();
    } else {
        send_created(sessionId);
    }
}

void XdgApplicationSessionV1InterfacePrivate::xdg_session_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void XdgApplicationSessionV1InterfacePrivate::xdg_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgApplicationSessionV1InterfacePrivate::xdg_session_v1_remove(Resource *resource)
{
    storage->remove(sessionId);
    wl_resource_destroy(resource->handle);
}

void XdgApplicationSessionV1InterfacePrivate::xdg_session_v1_get_toplevel_session(Resource *resource, uint32_t id, struct ::wl_resource *toplevel_resource, const QString &toplevel_id)
{
    XdgToplevelInterface *toplevel = XdgToplevelInterface::get(toplevel_resource);
    if (toplevel->session()) {
        wl_resource_post_error(resource->handle, error_already_exists, "xdg_toplevel already has a session");
        return;
    }

    if (toplevel->isConfigured()) {
        wl_resource_post_error(resource->handle, error_invalid_state, "xdg_toplevel is already initialized");
        return;
    }

    wl_resource *sessionResource = wl_resource_create(resource->client(),
                                                      &xdg_toplevel_session_v1_interface,
                                                      resource->version(), id);
    if (!sessionResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto session = new XdgToplevelSessionV1Interface(q, toplevel, toplevel_id, sessionResource);
    sessions.insert(toplevel_id, session);
    QObject::connect(session, &QObject::destroyed, q, [this, toplevel_id]() {
        sessions.remove(toplevel_id);
    });
}

void XdgApplicationSessionV1InterfacePrivate::xdg_session_v1_remove_toplevel_session(Resource *resource, const QString &toplevel_id)
{
    Q_UNUSED(resource)
    delete sessions.take(toplevel_id);
    storage->remove(sessionId, toplevel_id);
}

XdgApplicationSessionV1Interface::XdgApplicationSessionV1Interface(XdgSessionStorageV1 *storage, const QString &handle, wl_resource *resource)
    : d(new XdgApplicationSessionV1InterfacePrivate(storage, handle, resource, this))
{
}

XdgApplicationSessionV1Interface::~XdgApplicationSessionV1Interface()
{
}

XdgSessionStorageV1 *XdgApplicationSessionV1Interface::storage() const
{
    return d->storage;
}

QString XdgApplicationSessionV1Interface::sessionId() const
{
    return d->sessionId;
}

class XdgToplevelSessionV1InterfacePrivate : public QtWaylandServer::xdg_toplevel_session_v1
{
public:
    XdgToplevelSessionV1InterfacePrivate(XdgApplicationSessionV1Interface *session,
                                         XdgToplevelInterface *toplevel,
                                         const QString &toplevelId,
                                         wl_resource *resource,
                                         XdgToplevelSessionV1Interface *q);

    XdgToplevelSessionV1Interface *q;
    QPointer<XdgApplicationSessionV1Interface> session;
    QPointer<XdgToplevelInterface> toplevel;
    QString toplevelId;

protected:
    void xdg_toplevel_session_v1_destroy_resource(Resource *resource) override;
    void xdg_toplevel_session_v1_destroy(Resource *resource) override;
};

XdgToplevelSessionV1InterfacePrivate::XdgToplevelSessionV1InterfacePrivate(XdgApplicationSessionV1Interface *session,
                                                                           XdgToplevelInterface *toplevel,
                                                                           const QString &toplevelId,
                                                                           wl_resource *resource,
                                                                           XdgToplevelSessionV1Interface *q)
    : QtWaylandServer::xdg_toplevel_session_v1(resource)
    , q(q)
    , session(session)
    , toplevel(toplevel)
    , toplevelId(toplevelId)
{
}

void XdgToplevelSessionV1InterfacePrivate::xdg_toplevel_session_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void XdgToplevelSessionV1InterfacePrivate::xdg_toplevel_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

XdgToplevelSessionV1Interface::XdgToplevelSessionV1Interface(XdgApplicationSessionV1Interface *session,
                                                             XdgToplevelInterface *toplevel,
                                                             const QString &handle, wl_resource *resource)
    : d(new XdgToplevelSessionV1InterfacePrivate(session, toplevel, handle, resource, this))
{
    XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(toplevel);
    toplevelPrivate->session = this;
}

XdgToplevelSessionV1Interface::~XdgToplevelSessionV1Interface()
{
    if (d->toplevel) {
        XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(d->toplevel);
        toplevelPrivate->session = nullptr;
    }
}

bool XdgToplevelSessionV1Interface::isEmpty() const
{
    return false; // FIXME
}

XdgToplevelInterface *XdgToplevelSessionV1Interface::toplevel() const
{
    return d->toplevel;
}

QString XdgToplevelSessionV1Interface::toplevelId() const
{
    return d->toplevelId;
}

void XdgToplevelSessionV1Interface::sendRestored()
{
    d->send_restored();
}

QVariant XdgToplevelSessionV1Interface::read(const QString &key, const QVariant &defaultValue) const
{
    const QVariant data = d->session->storage()->read(d->session->sessionId(), d->toplevelId, key);
    if (data.isValid()) {
        return data;
    } else {
        return defaultValue;
    }
}

void XdgToplevelSessionV1Interface::write(const QString &key, const QVariant &value)
{
    d->session->storage()->write(d->session->sessionId(), d->toplevelId, key, value);
}

class XdgSessionConfigStorageV1Private
{
public:
    KSharedConfigPtr config;
};

XdgSessionConfigStorageV1::XdgSessionConfigStorageV1(QObject *parent)
    : XdgSessionStorageV1(parent)
    , d(new XdgSessionConfigStorageV1Private)
{
}

XdgSessionConfigStorageV1::XdgSessionConfigStorageV1(KSharedConfigPtr config, QObject *parent)
    : XdgSessionStorageV1(parent)
    , d(new XdgSessionConfigStorageV1Private)
{
    d->config = config;
}

XdgSessionConfigStorageV1::~XdgSessionConfigStorageV1()
{
}

KSharedConfigPtr XdgSessionConfigStorageV1::config() const
{
    return d->config;
}

void XdgSessionConfigStorageV1::setConfig(KSharedConfigPtr config)
{
    d->config = config;
}

bool XdgSessionConfigStorageV1::contains(const QString &sessionId) const
{
    return d->config->hasGroup(sessionId);
}

QVariant XdgSessionConfigStorageV1::read(const QString &sessionId, const QString &surfaceId, const QString &key) const
{
    const KConfigGroup sessionGroup(d->config, sessionId);
    const KConfigGroup surfaceGroup(&sessionGroup, surfaceId);

    QByteArray data = surfaceGroup.readEntry(key, QByteArray());
    if (data.isNull()) {
        return QVariant();
    }

    QDataStream stream(&data, QIODevice::ReadOnly);
    QVariant result;
    stream >> result;

    return result;
}

void XdgSessionConfigStorageV1::write(const QString &sessionId, const QString &surfaceId,
                                      const QString &key, const QVariant &value)
{
    KConfigGroup sessionGroup(d->config, sessionId);
    KConfigGroup surfaceGroup(&sessionGroup, surfaceId);

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << value;

    surfaceGroup.writeEntry(key, data);
}

void XdgSessionConfigStorageV1::remove(const QString &sessionId, const QString &surfaceId)
{
    KConfigGroup sessionGroup(d->config, sessionId);

    if (surfaceId.isEmpty()) {
        sessionGroup.deleteGroup();
    } else {
        KConfigGroup surfaceGroup(&sessionGroup, surfaceId);
        surfaceGroup.deleteGroup();
    }
}

void XdgSessionConfigStorageV1::sync()
{
    d->config->sync();
}

} // namespace KWaylandServer
