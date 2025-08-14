/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgsession_v1.h"

#include "display.h"
#include "xdgshell_p.h"

#include "qwayland-server-xx-session-management-v1.h"

#include <QDataStream>
#include <QIODevice>

namespace KWin
{

static const quint32 s_version = 1;

class XdgSessionManagerV1InterfacePrivate : public QtWaylandServer::xx_session_manager_v1
{
public:
    XdgSessionManagerV1InterfacePrivate(Display *display, std::unique_ptr<XdgSessionStorageV1> &&storage, XdgSessionManagerV1Interface *q);

    XdgSessionManagerV1Interface *q;
    QHash<QString, XdgApplicationSessionV1Interface *> sessions;
    std::unique_ptr<XdgSessionStorageV1> storage;

protected:
    void xx_session_manager_v1_destroy(Resource *resource) override;
    void xx_session_manager_v1_get_session(Resource *resource, uint32_t id, uint32_t reason, const QString &session) override;
};

XdgSessionManagerV1InterfacePrivate::XdgSessionManagerV1InterfacePrivate(Display *display,
                                                                         std::unique_ptr<XdgSessionStorageV1> &&storage,
                                                                         XdgSessionManagerV1Interface *q)
    : QtWaylandServer::xx_session_manager_v1(*display, s_version)
    , q(q)
    , storage(std::move(storage))
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

    auto applicationSession = new XdgApplicationSessionV1Interface(std::make_unique<XdgSessionDataV1>(storage.get(), sessionHandle), sessionHandle, resource->client(), id, resource->version());
    sessions.insert(sessionHandle, applicationSession);
    QObject::connect(applicationSession, &XdgApplicationSessionV1Interface::aboutToBeDestroyed, q, [this, applicationSession]() {
        if (!applicationSession->isReplaced()) {
            sessions.remove(applicationSession->sessionId());
        }
    });
}

XdgSessionManagerV1Interface::XdgSessionManagerV1Interface(Display *display, std::unique_ptr<XdgSessionStorageV1> &&storage, QObject *parent)
    : QObject(parent)
    , d(new XdgSessionManagerV1InterfacePrivate(display, std::move(storage), this))
{
}

XdgSessionManagerV1Interface::~XdgSessionManagerV1Interface()
{
}

class XdgApplicationSessionV1InterfacePrivate : public QtWaylandServer::xx_session_v1
{
public:
    XdgApplicationSessionV1InterfacePrivate(std::unique_ptr<XdgSessionDataV1> &&storage, const QString &sessionId, wl_client *client, int id, int version, XdgApplicationSessionV1Interface *q);

    XdgApplicationSessionV1Interface *q;
    std::unique_ptr<XdgSessionDataV1> storage;
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

XdgApplicationSessionV1InterfacePrivate::XdgApplicationSessionV1InterfacePrivate(std::unique_ptr<XdgSessionDataV1> &&storage,
                                                                                 const QString &sessionId,
                                                                                 wl_client *client, int id, int version,
                                                                                 XdgApplicationSessionV1Interface *q)
    : QtWaylandServer::xx_session_v1(client, id, version)
    , q(q)
    , storage(std::move(storage))
    , sessionId(sessionId)
{
    if (!this->storage->isEmpty()) {
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
        storage->remove();
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
        storage->remove(toplevel_id);
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

XdgApplicationSessionV1Interface::XdgApplicationSessionV1Interface(std::unique_ptr<XdgSessionDataV1> &&storage, const QString &handle, wl_client *client, int id, int version)
    : d(new XdgApplicationSessionV1InterfacePrivate(std::move(storage), handle, client, id, version, this))
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

XdgSessionDataV1 *XdgApplicationSessionV1Interface::storage() const
{
    return d->storage.get();
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
    d->storage.reset();
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
        session->storage()->remove(toplevelId);
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
        return d->session->storage()->contains(d->toplevelId);
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

QVariant XdgToplevelSessionV1Interface::read() const
{
    if (d->isInert()) {
        return QVariant();
    }

    return d->session->storage()->read(d->toplevelId);
}

void XdgToplevelSessionV1Interface::write(const QVariant &value)
{
    if (d->isInert()) {
        return;
    }

    d->session->storage()->write(d->toplevelId, value);
}

XdgSessionStorageV1::XdgSessionStorageV1(const QString &cacheName, unsigned defaultCacheSize, unsigned expectedItemSize)
    : m_store(std::make_unique<KSharedDataCache>(cacheName, defaultCacheSize, expectedItemSize))
{
    m_store->setEvictionPolicy(KSharedDataCache::EvictOldest);
}

XdgSessionStorageV1::~XdgSessionStorageV1()
{
}

KSharedDataCache *XdgSessionStorageV1::store() const
{
    return m_store.get();
}

class XdgSessionDataV1Private
{
public:
    XdgSessionDataV1Private(XdgSessionStorageV1 *storage, const QString &sessionId);

    void load();
    void sync();

    XdgSessionStorageV1 *m_storage;
    QString m_sessionId;
    QVariantHash m_sessionMap;
    bool m_dirty = false;
};

XdgSessionDataV1Private::XdgSessionDataV1Private(XdgSessionStorageV1 *storage, const QString &sessionId)
    : m_storage(storage)
    , m_sessionId(sessionId)
{
}

void XdgSessionDataV1Private::load()
{
    QByteArray rawData;
    if (!m_storage->store()->find(m_sessionId, &rawData)) {
        return;
    }

    QDataStream stream(rawData);
    QVariant result;
    stream >> m_sessionMap;
}

void XdgSessionDataV1Private::sync()
{
    if (!m_dirty) {
        return;
    }

    m_dirty = false;

    if (m_sessionMap.isEmpty()) {
        m_storage->store()->remove(m_sessionId);
    } else {
        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);
        stream << m_sessionMap;

        m_storage->store()->insert(m_sessionId, byteArray);
    }
}

XdgSessionDataV1::XdgSessionDataV1(XdgSessionStorageV1 *storage, const QString &sessionId)
    : d(std::make_unique<XdgSessionDataV1Private>(storage, sessionId))
{
    d->load();
}

XdgSessionDataV1::~XdgSessionDataV1()
{
    d->sync();
}

bool XdgSessionDataV1::isEmpty() const
{
    return d->m_sessionMap.isEmpty();
}

bool XdgSessionDataV1::contains(const QString &toplevelId) const
{
    return d->m_sessionMap.contains(toplevelId);
}

QVariant XdgSessionDataV1::read(const QString &toplevelId) const
{
    return d->m_sessionMap.value(toplevelId);
}

void XdgSessionDataV1::write(const QString &toplevelId, const QVariant &value)
{
    QVariant &reference = d->m_sessionMap[toplevelId];
    if (reference != value) {
        d->m_dirty = true;
        reference = value;
    }
}

void XdgSessionDataV1::remove()
{
    if (!d->m_sessionMap.isEmpty()) {
        d->m_dirty = true;
        d->m_sessionMap.clear();
    }
}

void XdgSessionDataV1::remove(const QString &surfaceId)
{
    if (d->m_sessionMap.contains(surfaceId)) {
        d->m_dirty = true;
        d->m_sessionMap.remove(surfaceId);
    }
}

} // namespace KWin
