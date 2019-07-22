/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_DBUSCALL_H
#define KWIN_DBUSCALL_H

#include <QObject>
#include <QString>

namespace KWin
{

/**
 * @brief Qml export for providing a wrapper for sending a message over the DBus
 * session bus.
 *
 * Allows to setup the connection arguments just like in QDBusMessage and supports
 * adding arguments to the call. To invoke the message use the slot @ref call.
 *
 * If the call succeeds the signal @ref finished is emitted, if the call fails
 * the signal @ref failed is emitted.
 *
 * Note: the DBusCall always uses the session bus and performs an async call.
 *
 * Example on how to use in Qml:
 * @code
 * DBusCall {
 *     id: dbus
 *     service: "org.kde.KWin"
 *     path: "/KWin"
 *     method: "nextDesktop"
 *     Component.onCompleted: dbus.call()
 * }
 * @endcode
 *
 * Example with arguments:
 * @code
 * DBusCall {
 *     id: dbus
 *     service: "org.kde.KWin"
 *     path: "/KWin"
 *     method: "setCurrentDesktop"
 *     arguments: [1]
 *     Component.onCompleted: dbus.call()
 * }
 * @endcode
 *
 * Example with a callback:
 * @code
 * DBusCall {
 *      id: dbus
 *      service: "org.kde.KWin"
 *      path: "/KWin"
 *      method: "currentDesktop"
 *      onFinished: console.log(returnValue[0])
 *  }
 * @endcode
 **/
class DBusCall : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY serviceChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QString dbusInterface READ interface WRITE setInterface NOTIFY interfaceChanged)
    Q_PROPERTY(QString method READ method WRITE setMethod NOTIFY methodChanged)
    Q_PROPERTY(QVariantList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
public:
    explicit DBusCall(QObject* parent = 0);
    ~DBusCall() override;

    const QString &service() const;
    const QString &path() const;
    const QString &interface() const;
    const QString &method() const;
    const QVariantList &arguments() const;

public Q_SLOTS:
    void call();

    void setService(const QString &service);
    void setPath(const QString &path);
    void setInterface(const QString &interface);
    void setMethod(const QString &method);
    void setArguments(const QVariantList &arguments);

Q_SIGNALS:
    void finished(QVariantList returnValue);
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

#define GENERIC_WRAPPER(type, name, upperName) \
inline type DBusCall::name() const \
{ \
    return m_##name; \
}\
inline void DBusCall::set##upperName(type name) \
{\
    if (m_##name == name) { \
        return; \
    } \
    m_##name = name; \
    emit name##Changed(); \
}
#define WRAPPER(name, upperName) \
GENERIC_WRAPPER(const QString&, name, upperName)

WRAPPER(interface, Interface)
WRAPPER(method, Method)
WRAPPER(path, Path)
WRAPPER(service, Service)

GENERIC_WRAPPER(const QVariantList &, arguments, Arguments)
#undef WRAPPER
#undef GENERIC_WRAPPER

} // KWin

#endif //  KWIN_SCRIPTING_DBUSCALL_H
