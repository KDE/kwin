/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <kwin_export.h>

#include <QObject>

#include <KSharedConfig>
#include <kconfigconversioncheck_p.h>

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
    explicit XdgSessionStorageV1(QObject *parent = nullptr);
    ~XdgSessionStorageV1() override;

    std::unique_ptr<XdgSessionDataV1> session(const QString &sessionId);
};

class KWIN_EXPORT XdgSessionDataV1 : public QObject
{
    Q_OBJECT

public:
    explicit XdgSessionDataV1(const QString &filePath);
    ~XdgSessionDataV1() override;

    bool isRestored() const;
    bool contains(const QString &toplevelId) const;
    QVariant read(const QString &toplevelId, const QString &key, const QMetaType &metaType) const;
    void write(const QString &toplevelId, const QString &key, const QVariant &value);
    void remove();
    void remove(const QString &toplevelId);

private:
    KSharedConfigPtr m_config;
    QString m_filePath;
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
     * Returns the value for the property @a key. If the session storage doesn't contain any
     * property with the specified key, this function returns @c std::nullopt.
     */
    template<typename T>
    std::optional<T> read(const QString &key) const;

    /**
     * Sets the value of property @a key to @a value. If the key already exists, the previous
     * value is overwritten.
     */
    template<typename T>
    void write(const QString &key, const T &value);

private:
    QVariant rawRead(const QString &key, const QMetaType &metaType) const;
    void rawWrite(const QString &key, const QVariant &value);

    std::unique_ptr<XdgToplevelSessionV1InterfacePrivate> d;
    friend class XdgToplevelSessionV1InterfacePrivate;
};

template<typename T>
std::optional<T> XdgToplevelSessionV1Interface::read(const QString &key) const
{
    KConfigConversionCheck::to_QVariant<T>();
    const QVariant value = rawRead(key, QMetaType::fromType<T>());
    if (value.isNull()) {
        return std::nullopt;
    } else {
        return value.value<T>();
    }
}

template<typename T>
void XdgToplevelSessionV1Interface::write(const QString &key, const T &value)
{
    KConfigConversionCheck::to_QVariant<T>();
    rawWrite(key, QVariant::fromValue(value));
}

} // namespace KWin
