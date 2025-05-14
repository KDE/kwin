/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace KWin
{

/*!
 * \qmltype DBusCall
 * \inqmlmodule org.kde.kwin
 *
 * \brief Qml export for providing a wrapper for sending a message over the DBus
 * session bus.
 *
 * Allows to setup the connection arguments just like in QDBusMessage and supports
 * adding arguments to the call. To invoke the message use the slot call().
 *
 * If the call succeeds the signal finished() is emitted, if the call fails
 * the signal failed() is emitted.
 *
 * Note: the DBusCall always uses the session bus and performs an async call.
 *
 * Example on how to use in Qml:
 * \code
 * DBusCall {
 *     id: dbus
 *     service: "org.kde.KWin"
 *     path: "/KWin"
 *     method: "nextDesktop"
 *     Component.onCompleted: dbus.call()
 * }
 * \endcode
 *
 * Example with arguments:
 * \code
 * DBusCall {
 *     id: dbus
 *     service: "org.kde.KWin"
 *     path: "/KWin"
 *     method: "setCurrentDesktop"
 *     arguments: [1]
 *     Component.onCompleted: dbus.call()
 * }
 * \endcode
 *
 * Example with a callback:
 * \code
 * DBusCall {
 *      id: dbus
 *      service: "org.kde.KWin"
 *      path: "/KWin"
 *      method: "currentDesktop"
 *      onFinished: console.log(returnValue[0])
 *  }
 * \endcode
 */
class DBusCall : public QObject
{
    Q_OBJECT

    /*!
     * \qmlproperty string DBusCall::service
     *
     * This property specifies the dbus service name of the remote object. (service, path, dbusInterface, method)
     * tuplet forms the destination address.
     */
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY serviceChanged)

    /*!
     * \qmlproperty string DBusCall::path
     *
     * This property specifies the dbus object path. (service, path, dbusInterface, method) tuplet
     * forms the destination address.
     */
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)

    /*!
     * \qmlproperty string DBusCall::dbusInterface
     *
     * This property specifies the dbus interface. (service, path, dbusInterface, method) tuplet forms
     * the destination address.
     */
    Q_PROPERTY(QString dbusInterface READ interface WRITE setInterface NOTIFY interfaceChanged)

    /*!
     * \qmlproperty string DBusCall::method
     *
     * This property specifies the name of the method. (service, path, dbusInterface, method) tuplet
     * forms the destination address.
     */
    Q_PROPERTY(QString method READ method WRITE setMethod NOTIFY methodChanged)

    /*!
     * \qmlproperty string DBusCall::arguments
     *
     * This property specifies the arguments that will be passed to the request.
     */
    Q_PROPERTY(QVariantList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
public:
    explicit DBusCall(QObject *parent = nullptr);
    ~DBusCall() override;

    const QString &service() const;
    const QString &path() const;
    const QString &interface() const;
    const QString &method() const;
    const QVariantList &arguments() const;

public Q_SLOTS:
    /*!
     * Calls the specified method asynchronously. If the method call succeeds, the finished() signal
     * will be emitted. If the method call fails, the failed() signal will be emitted.
     *
     * \sa finished(), failed()
     */
    void call();

    void setService(const QString &service);
    void setPath(const QString &path);
    void setInterface(const QString &interface);
    void setMethod(const QString &method);
    void setArguments(const QVariantList &arguments);

Q_SIGNALS:
    /*!
     * \qmlsignal DBusCall::finished(list<variant> returnValue)
     *
     * This signal is emitted if a dbus method call finishes successfully. The \a returnValue
     * specifies an optional return value.
     */
    void finished(QVariantList returnValue);

    /*!
     * \qmlsignal DBusCall::failed()
     *
     * This signal is emitted if a dbus method call fails.
     */
    void failed();

    void serviceChanged();
    void pathChanged();
    void interfaceChanged();
    void methodChanged();
    void argumentsChanged();

private:
    QString m_service;
    QString m_path;
    QString m_interface;
    QString m_method;
    QVariantList m_arguments;
};

#define GENERIC_WRAPPER(type, name, upperName)      \
    inline type DBusCall::name() const              \
    {                                               \
        return m_##name;                            \
    }                                               \
    inline void DBusCall::set##upperName(type name) \
    {                                               \
        if (m_##name == name) {                     \
            return;                                 \
        }                                           \
        m_##name = name;                            \
        Q_EMIT name##Changed();                     \
    }
#define WRAPPER(name, upperName) \
    GENERIC_WRAPPER(const QString &, name, upperName)

WRAPPER(interface, Interface)
WRAPPER(method, Method)
WRAPPER(path, Path)
WRAPPER(service, Service)

GENERIC_WRAPPER(const QVariantList &, arguments, Arguments)
#undef WRAPPER
#undef GENERIC_WRAPPER

} // KWin
