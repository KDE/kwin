/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>
#include <QScopedPointer>
#include <KWaylandServer/kwaylandserver_export.h>
#include <wayland-server.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class OutputInterface;
class ScreencastInterfacePrivate;
class ScreencastStreamInterfacePrivate;
class ScreencastStreamInterface;

class KWAYLANDSERVER_EXPORT ScreencastStreamInterface : public QObject
{
    Q_OBJECT
public:
    ~ScreencastStreamInterface() override;

    void sendCreated(quint32 nodeid);
    void sendFailed(const QString &error);
    void sendClosed();

Q_SIGNALS:
    void finished();

private:
    friend class ScreencastInterfacePrivate;
    explicit ScreencastStreamInterface(QObject *parent);
    QScopedPointer<ScreencastStreamInterfacePrivate> d;
};

class KWAYLANDSERVER_EXPORT ScreencastInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~ScreencastInterface();

    enum CursorMode {
        Hidden = 1,
        Embedded = 2,
        Metadata = 4,
    };
    Q_ENUM(CursorMode);


Q_SIGNALS:
    void outputScreencastRequested(ScreencastStreamInterface* stream, ::wl_resource *output, CursorMode mode);
    void windowScreencastRequested(ScreencastStreamInterface* stream, const QString &winid, CursorMode mode);

private:
    explicit ScreencastInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    QScopedPointer<ScreencastInterfacePrivate> d;
};

}
