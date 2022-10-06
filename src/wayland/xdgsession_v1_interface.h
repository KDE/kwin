/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <kwin_export.h>

#include <QObject>

#include <KSharedConfig>

#include <memory>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class XdgApplicationSessionV1InterfacePrivate;
class XdgSessionConfigStorageV1Private;
class XdgSessionManagerV1InterfacePrivate;
class XdgToplevelInterface;
class XdgToplevelSessionV1Interface;
class XdgToplevelSessionV1InterfacePrivate;

/**
 * The XdgSessionStorageV1 class represents the storage for the compositor's session data.
 *
 * XdgSessionStorageV1 stores toplevel surface session data such as the frame geometry, the
 * maximize mode, etc. No restrictions are imposed on the data type, the compositor can store
 * data of any kind in the storage, for example QRect, QSize, QString, etc.
 *
 * Note that it is the responsibility of the compositor to decide when the storage must be
 * sync'ed.
 */
class KWIN_EXPORT XdgSessionStorageV1 : public QObject
{
    Q_OBJECT

public:
    explicit XdgSessionStorageV1(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual bool contains(const QString &sessionId) const = 0;
    virtual QVariant read(const QString &sessionId, const QString &toplevelId, const QString &key) const = 0;
    virtual void write(const QString &sessionid, const QString &toplevelId, const QString &key, const QVariant &value) = 0;
    virtual void remove(const QString &sessionId, const QString &toplevelId = QString()) = 0;
    virtual void sync() = 0;
};

/**
 * The XdgSessionConfigStorageV1 class represents a session storage backed by a KConfig.
 */
class KWIN_EXPORT XdgSessionConfigStorageV1 : public XdgSessionStorageV1
{
    Q_OBJECT

public:
    /**
     * Constructs a XdgSessionConfigStorageV1 with the specified @a parent and no config.
     */
    explicit XdgSessionConfigStorageV1(QObject *parent = nullptr);

    /**
     * Constructs a XdgSessionConfigStorageV1 with the specified @a config and @a parent.
     */
    explicit XdgSessionConfigStorageV1(KSharedConfigPtr config, QObject *parent = nullptr);

    /**
     * Destructs the XdgSessionConfigStorageV1 object without synchronizing the config.
     */
    ~XdgSessionConfigStorageV1() override;

    /**
     * Returns the config object attached to this session storage.
     */
    KSharedConfigPtr config() const;

    /**
     * Sets the config object for this session storage to @a config.
     */
    void setConfig(KSharedConfigPtr config);

    bool contains(const QString &sessionId) const override;
    QVariant read(const QString &sessionId, const QString &toplevelId, const QString &key) const override;
    void write(const QString &sessionId, const QString &toplevelId, const QString &key, const QVariant &value) override;
    void remove(const QString &sessionId, const QString &toplevelId = QString()) override;
    void sync() override;

private:
    std::unique_ptr<XdgSessionConfigStorageV1Private> d;
};

/**
 * The XdgSessionManagerV1Interface compositor extension that allows clients to create sessions
 * for toplevel surfaces that persist across compositor and application restarts.
 *
 * The XdgSessionManagerV1Interface corresponds to the Wayland interface @c zxdg_session_manager_v1.
 */
class KWIN_EXPORT XdgSessionManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgSessionManagerV1Interface(Display *display, XdgSessionStorageV1 *storage, QObject *parent = nullptr);
    ~XdgSessionManagerV1Interface() override;

    /**
     * Returns the backing storage for the compositor's session data.
     */
    XdgSessionStorageV1 *storage() const;

private:
    std::unique_ptr<XdgSessionManagerV1InterfacePrivate> d;
    friend class XdgSessionManagerV1InterfacePrivate;
};

/**
 * The XdgApplicationSessionV1Interface class represents a session for an application.
 *
 * The XdgApplicationSessionV1Interface corresponds to the Wayland interface @c zxdg_session_v1.
 */
class KWIN_EXPORT XdgApplicationSessionV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgApplicationSessionV1Interface(XdgSessionStorageV1 *storage, const QString &handle, wl_resource *resource);
    ~XdgApplicationSessionV1Interface() override;

    /**
     * Returns the session storage for this application session.
     */
    XdgSessionStorageV1 *storage() const;

    /**
     * Returns the handle that uniquely identifies this application session object.
     */
    QString sessionId() const;

private:
    std::unique_ptr<XdgApplicationSessionV1InterfacePrivate> d;
    friend class XdgApplicationSessionV1InterfacePrivate;
};

/**
 * The XdgToplevelSessionV1Interface class represents a session for an xdg_toplevel surface.
 *
 * The XdgToplevelSessionV1Interface corresponds to the Wayland interface @c zxdg_toplevel_session_v1.
 */
class KWIN_EXPORT XdgToplevelSessionV1Interface : public QObject
{
    Q_OBJECT

public:
    XdgToplevelSessionV1Interface(XdgApplicationSessionV1Interface *session,
                                  XdgToplevelInterface *toplevel,
                                  const QString &handle, wl_resource *resource);
    ~XdgToplevelSessionV1Interface() override;

    /**
     * Returns @c true if this session is empty; otherwise returns @c false. A new session is empty.
     */
    bool isEmpty() const;

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
     * Returns the value for the property @a key. If the session storage doesn't contain any
     * property with the specified key, this function returns @a defaultValue.
     */
    QVariant read(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * Sets the value of property @a key to @a value. If the key already exists, the previous
     * value is overwritten.
     */
    void write(const QString &key, const QVariant &value);

private:
    std::unique_ptr<XdgToplevelSessionV1InterfacePrivate> d;
    friend class XdgToplevelSessionV1InterfacePrivate;
};

} // namespace KWaylandServer
