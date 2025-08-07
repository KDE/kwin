/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgsession_v1.h"

#include "display.h"
#include "xdgshell_p.h"

#include "qwayland-server-xx-session-management-v1.h"

#include <KConfigGroup>
#include <QIODevice>

namespace KWin
{

static const quint32 s_version = 1;

class XdgSessionManagerV1InterfacePrivate : public QtWaylandServer::xx_session_manager_v1
{
public:
    XdgSessionManagerV1InterfacePrivate(Display *display, XdgSessionStorageV1 *storage, XdgSessionManagerV1Interface *q);

    XdgSessionManagerV1Interface *q;
    QHash<QString, XdgApplicationSessionV1Interface *> sessions;
    XdgSessionStorageV1 *storage;

protected:
    void xx_session_manager_v1_destroy(Resource *resource) override;
    void xx_session_manager_v1_get_session(Resource *resource, uint32_t id, uint32_t reason, const QString &session) override;
};

XdgSessionManagerV1InterfacePrivate::XdgSessionManagerV1InterfacePrivate(Display *display,
                                                                         XdgSessionStorageV1 *storage,
                                                                         XdgSessionManagerV1Interface *q)
    : QtWaylandServer::xx_session_manager_v1(*display, s_version)
    , q(q)
    , storage(storage)
{
}

void XdgSessionManagerV1InterfacePrivate::xx_session_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgSessionManagerV1InterfacePrivate::xx_session_manager_v1_get_session(Resource *resource, uint32_t id, uint32_t reason, const QString &session)
{
    QString sessionHandle = session;
    if (sessionHandle.isEmpty()) {
        sessionHandle = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (auto it = sessions.find(sessionHandle); it != sessions.end()) {
        XdgApplicationSessionV1Interface *session = *it;
        if (session->client() == resource->client()) {
            wl_resource_post_error(resource->handle, error_in_use, "%s session is already in use", qPrintable(sessionHandle));
            return;
        }

        // The session has been taken over by another client.
        session->markReplaced();
    }

    auto applicationSession = new XdgApplicationSessionV1Interface(storage, sessionHandle, resource->client(), id, resource->version());
    sessions.insert(sessionHandle, applicationSession);
    QObject::connect(applicationSession, &XdgApplicationSessionV1Interface::aboutToBeDestroyed, q, [this, applicationSession]() {
        if (!applicationSession->isReplaced()) {
            sessions.remove(applicationSession->sessionId());
        }
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

class XdgApplicationSessionV1InterfacePrivate : public QtWaylandServer::xx_session_v1
{
public:
    XdgApplicationSessionV1InterfacePrivate(XdgSessionStorageV1 *storage, const QString &sessionId, wl_client *client, int id, int version, XdgApplicationSessionV1Interface *q);

    XdgApplicationSessionV1Interface *q;
    XdgSessionStorageV1 *storage;
    QHash<QString, XdgToplevelSessionV1Interface *> sessions;
    QString sessionId;
    bool replaced = false;

protected:
    void xx_session_v1_destroy_resource(Resource *resource) override;
    void xx_session_v1_destroy(Resource *resource) override;
    void xx_session_v1_remove(Resource *resource) override;
    void xx_session_v1_add_toplevel(Resource *resource, uint32_t id, struct ::wl_resource *toplevel, const QString &toplevel_id) override;
    void xx_session_v1_restore_toplevel(Resource *resource, uint32_t id, wl_resource *toplevel, const QString &toplevel_id) override;
};

XdgApplicationSessionV1InterfacePrivate::XdgApplicationSessionV1InterfacePrivate(XdgSessionStorageV1 *storage,
                                                                                 const QString &sessionId,
                                                                                 wl_client *client, int id, int version,
                                                                                 XdgApplicationSessionV1Interface *q)
    : QtWaylandServer::xx_session_v1(client, id, version)
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

void XdgApplicationSessionV1InterfacePrivate::xx_session_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgApplicationSessionV1InterfacePrivate::xx_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgApplicationSessionV1InterfacePrivate::xx_session_v1_remove(Resource *resource)
{
    if (!replaced) {
        storage->remove(sessionId);
    }
    wl_resource_destroy(resource->handle);
}

void XdgApplicationSessionV1InterfacePrivate::xx_session_v1_add_toplevel(Resource *resource, uint32_t id, struct ::wl_resource *toplevel_resource, const QString &toplevel_id)
{
    XdgToplevelInterface *toplevel = XdgToplevelInterface::get(toplevel_resource);
    if (toplevel->session()) {
        wl_resource_post_error(resource->handle, error_name_in_use, "xdg_toplevel already has a session");
        return;
    }

    if (sessions.contains(toplevel_id)) {
        wl_resource_post_error(resource->handle, error_name_in_use, "the specified toplevel id is already used");
        return;
    }

    // clear any storage, this ensures we won't restore anything
    if (!replaced) {
        storage->remove(sessionId, toplevel_id);
    }

    auto session = new XdgToplevelSessionV1Interface(q, toplevel, toplevel_id, resource->client(), id, resource->version());
    sessions.insert(toplevel_id, session);
    QObject::connect(session, &QObject::destroyed, q, [this, toplevel_id]() {
        sessions.remove(toplevel_id);
    });
}

void XdgApplicationSessionV1InterfacePrivate::xx_session_v1_restore_toplevel(Resource *resource, uint32_t id, wl_resource *toplevel_resource, const QString &toplevel_id)
{
    XdgToplevelInterface *toplevel = XdgToplevelInterface::get(toplevel_resource);
    if (toplevel->session()) {
        wl_resource_post_error(resource->handle, error_name_in_use, "xdg_toplevel already has a session");
        return;
    }

    if (toplevel->isConfigured()) {
        wl_resource_post_error(resource->handle, error_already_mapped, "xdg_toplevel is already initialized");
        return;
    }

    if (sessions.contains(toplevel_id)) {
        wl_resource_post_error(resource->handle, error_name_in_use, "the specified toplevel id is already used");
        return;
    }

    auto session = new XdgToplevelSessionV1Interface(q, toplevel, toplevel_id, resource->client(), id, resource->version());
    sessions.insert(toplevel_id, session);
    QObject::connect(session, &QObject::destroyed, q, [this, toplevel_id]() {
        sessions.remove(toplevel_id);
    });
}

XdgApplicationSessionV1Interface::XdgApplicationSessionV1Interface(XdgSessionStorageV1 *storage, const QString &handle, wl_client *client, int id, int version)
    : d(new XdgApplicationSessionV1InterfacePrivate(storage, handle, client, id, version, this))
{
}

XdgApplicationSessionV1Interface::~XdgApplicationSessionV1Interface()
{
    Q_EMIT aboutToBeDestroyed();
}

wl_client *XdgApplicationSessionV1Interface::client() const
{
    return d->resource()->client();
}

XdgSessionStorageV1 *XdgApplicationSessionV1Interface::storage() const
{
    return d->storage;
}

QString XdgApplicationSessionV1Interface::sessionId() const
{
    return d->sessionId;
}

bool XdgApplicationSessionV1Interface::isReplaced() const
{
    return d->replaced;
}

void XdgApplicationSessionV1Interface::markReplaced()
{
    d->replaced = true;
    d->send_replaced();
}

class XdgToplevelSessionV1InterfacePrivate : public QtWaylandServer::xx_toplevel_session_v1
{
public:
    XdgToplevelSessionV1InterfacePrivate(XdgApplicationSessionV1Interface *session,
                                         XdgToplevelInterface *toplevel,
                                         const QString &toplevelId,
                                         wl_client *client, int id, int version,
                                         XdgToplevelSessionV1Interface *q);

    XdgToplevelSessionV1Interface *q;
    QPointer<XdgApplicationSessionV1Interface> session;
    QPointer<XdgToplevelInterface> toplevel;
    QString toplevelId;

    bool isInert() const;

protected:
    void xx_toplevel_session_v1_destroy_resource(Resource *resource) override;
    void xx_toplevel_session_v1_destroy(Resource *resource) override;
    void xx_toplevel_session_v1_remove(Resource *resource) override;
};

XdgToplevelSessionV1InterfacePrivate::XdgToplevelSessionV1InterfacePrivate(XdgApplicationSessionV1Interface *session,
                                                                           XdgToplevelInterface *toplevel,
                                                                           const QString &toplevelId,
                                                                           wl_client *client, int id, int version,
                                                                           XdgToplevelSessionV1Interface *q)
    : QtWaylandServer::xx_toplevel_session_v1(client, id, version)
    , q(q)
    , session(session)
    , toplevel(toplevel)
    , toplevelId(toplevelId)
{
}

bool XdgToplevelSessionV1InterfacePrivate::isInert() const
{
    return !session || session->isReplaced();
}

void XdgToplevelSessionV1InterfacePrivate::xx_toplevel_session_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgToplevelSessionV1InterfacePrivate::xx_toplevel_session_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelSessionV1InterfacePrivate::xx_toplevel_session_v1_remove(Resource *resource)
{
    if (!isInert()) {
        session->storage()->remove(session->sessionId(), toplevelId);
    }
    wl_resource_destroy(resource->handle);
}

XdgToplevelSessionV1Interface::XdgToplevelSessionV1Interface(XdgApplicationSessionV1Interface *session,
                                                             XdgToplevelInterface *toplevel,
                                                             const QString &handle, wl_client *client, int id, int version)
    : d(new XdgToplevelSessionV1InterfacePrivate(session, toplevel, handle, client, id, version, this))
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

bool XdgToplevelSessionV1Interface::exists() const
{
    if (d->isInert()) {
        return false;
    } else {
        return d->session->storage()->contains(d->session->sessionId(), d->toplevelId);
    }
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
    if (d->isInert()) {
        return;
    }

    if (!d->toplevel) {
        return;
    }
    // FEEDBACK: This API can be improved, the client knows the toplevel
    d->send_restored(d->toplevel->resource());
}

QVariant XdgToplevelSessionV1Interface::read(const QString &key, const QVariant &defaultValue) const
{
    if (d->isInert()) {
        return QVariant();
    }

    const QVariant data = d->session->storage()->read(d->session->sessionId(), d->toplevelId, key);
    if (data.isValid()) {
        return data;
    } else {
        return defaultValue;
    }
}

void XdgToplevelSessionV1Interface::write(const QString &key, const QVariant &value)
{
    if (d->isInert()) {
        return;
    }

    d->session->storage()->write(d->session->sessionId(), d->toplevelId, key, value);
}

class XdgSessionStorageV1Private
{
public:
    KSharedConfigPtr config;
};

XdgSessionStorageV1::XdgSessionStorageV1(QObject *parent)
    : QObject(parent)
    , d(new XdgSessionStorageV1Private)
{
}

XdgSessionStorageV1::XdgSessionStorageV1(KSharedConfigPtr config, QObject *parent)
    : QObject(parent)
    , d(new XdgSessionStorageV1Private)
{
    d->config = config;
}

XdgSessionStorageV1::~XdgSessionStorageV1()
{
}

KSharedConfigPtr XdgSessionStorageV1::config() const
{
    return d->config;
}

void XdgSessionStorageV1::setConfig(KSharedConfigPtr config)
{
    d->config = config;
}

bool XdgSessionStorageV1::contains(const QString &sessionId, const QString &toplevelId) const
{
    if (toplevelId.isEmpty()) {
        return d->config->hasGroup(sessionId);
    } else {
        return d->config->group(sessionId).hasGroup(toplevelId);
    }
}

QVariant XdgSessionStorageV1::read(const QString &sessionId, const QString &surfaceId, const QString &key) const
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

void XdgSessionStorageV1::write(const QString &sessionId, const QString &surfaceId,
                                const QString &key, const QVariant &value)
{
    KConfigGroup sessionGroup(d->config, sessionId);
    KConfigGroup surfaceGroup(&sessionGroup, surfaceId);

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << value;

    surfaceGroup.writeEntry(key, data);
}

void XdgSessionStorageV1::remove(const QString &sessionId, const QString &surfaceId)
{
    KConfigGroup sessionGroup(d->config, sessionId);

    if (surfaceId.isEmpty()) {
        sessionGroup.deleteGroup();
    } else {
        KConfigGroup surfaceGroup(&sessionGroup, surfaceId);
        surfaceGroup.deleteGroup();
    }
}

void XdgSessionStorageV1::sync()
{
    d->config->sync();
}

} // namespace KWin
