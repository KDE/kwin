/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <kwin_export.h>

#include <KSharedDataCache>

#include <QObject>
#include <QVariant>

#include <memory>

struct wl_client;

namespace KWin
{

class Display;
class XdgApplicationSessionV1InterfacePrivate;
class XdgSessionDataV1;
class XdgSessionManagerV1InterfacePrivate;
class XdgToplevelInterface;
class XdgToplevelSessionV1Interface;
class XdgToplevelSessionV1InterfacePrivate;
class XdgSessionDataV1Private;

/**
 * The XdgSessionStorageV1 class represents the storage for the compositor's session data.
 *
 * XdgSessionStorageV1 stores toplevel surface session data such as the frame geometry, the
 * maximize mode, etc. No restrictions are imposed on the data type, the compositor can store
 * data of any kind in the storage, for example QRect, QSize, QString, etc.
 */
class KWIN_EXPORT XdgSessionStorageV1
{
public:
    XdgSessionStorageV1(const QString &cacheName, unsigned defaultCacheSize, unsigned expectedItemSize = 0);
    ~XdgSessionStorageV1();

    KSharedDataCache *store() const;

private:
    std::unique_ptr<KSharedDataCache> m_store;
};

/**
 * The XdgSessionDataV1 type represents data associated with an xdg session.
 */
class KWIN_EXPORT XdgSessionDataV1
{
public:
    explicit XdgSessionDataV1(XdgSessionStorageV1 *store, const QString &sessionId);
    ~XdgSessionDataV1();

    bool isEmpty() const;
    bool contains(const QString &toplevelId) const;
    QVariant read(const QString &toplevelId) const;
    void write(const QString &toplevelId, const QVariant &value);
    void remove();
    void remove(const QString &toplevelId);

private:
    std::unique_ptr<XdgSessionDataV1Private> d;
};

/**
 * The XdgSessionManagerV1Interface compositor extension that allows clients to create sessions
 * for toplevel surfaces that persist across compositor and application restarts.
 *
 * The XdgSessionManagerV1Interface corresponds to the Wayland interface @c xx_session_manager_v1.
 */
class KWIN_EXPORT XdgSessionManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgSessionManagerV1Interface(Display *display, std::unique_ptr<XdgSessionStorageV1> &&storage, QObject *parent = nullptr);
    ~XdgSessionManagerV1Interface() override;

private:
    std::unique_ptr<XdgSessionManagerV1InterfacePrivate> d;
    friend class XdgSessionManagerV1InterfacePrivate;
};

/**
 * The XdgApplicationSessionV1Interface class represents a session for an application.
 *
 * The XdgApplicationSessionV1Interface corresponds to the Wayland interface @c xx_session_v1.
 */
class KWIN_EXPORT XdgApplicationSessionV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgApplicationSessionV1Interface(std::unique_ptr<XdgSessionDataV1> &&storage, const QString &handle, wl_client *client, int id, int version);
    ~XdgApplicationSessionV1Interface() override;

    /**
     * Returns the client that owns the session object.
     */
    wl_client *client() const;

    /**
     * Returns the session storage for this application session.
     */
    XdgSessionDataV1 *storage() const;

    /**
     * Returns the handle that uniquely identifies this application session object.
     */
    QString sessionId() const;

    /**
     * Returns @c true if the session has been taken over by another client; otherwise returns @c false.
     */
    bool isReplaced() const;
    void markReplaced();

Q_SIGNALS:
    void aboutToBeDestroyed();

private:
    std::unique_ptr<XdgApplicationSessionV1InterfacePrivate> d;
    friend class XdgApplicationSessionV1InterfacePrivate;
};

/**
 * The XdgToplevelSessionV1Interface class represents a session for an xdg_toplevel surface.
 *
 * The XdgToplevelSessionV1Interface corresponds to the Wayland interface @c xx_toplevel_session_v1.
 */
class KWIN_EXPORT XdgToplevelSessionV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgToplevelSessionV1Interface(XdgApplicationSessionV1Interface *session,
                                  XdgToplevelInterface *toplevel,
                                  const QString &handle, wl_client *client, int id, int version);
    ~XdgToplevelSessionV1Interface() override;

    /**
     * Returns @c true if the toplevel session has some stored data; otherwise returns @c false.
     * If it is a new toplevel session, this function will return @c false.
     */
    bool exists() const;

    /**
     * Returns the XdgToplevelInterface object associated with this toplevel session.
     */
    XdgToplevelInterface *toplevel() const;

    /**
     * Returns the handle that uniquely identifies this toplevel session object.
     */
    QString toplevelId() const;

    /**
     * Indicate whether this toplevel session has been used to restore the state of the surface.
     */
    void sendRestored();

    /**
     * Returns the data stored for this session.
     */
    QVariant read() const;

    /**
     * Stores the specified @a value for this session.
     */
    void write(const QVariant &value);

private:
    std::unique_ptr<XdgToplevelSessionV1InterfacePrivate> d;
    friend class XdgToplevelSessionV1InterfacePrivate;
};

} // namespace KWin
