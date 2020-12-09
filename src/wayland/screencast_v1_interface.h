/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>
#include <QScopedPointer>
#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class OutputInterface;
class ScreencastV1InterfacePrivate;
class ScreencastStreamV1InterfacePrivate;
class ScreencastStreamV1Interface;

class KWAYLANDSERVER_EXPORT ScreencastStreamV1Interface : public QObject
{
    Q_OBJECT
public:
    ~ScreencastStreamV1Interface() override;

    void sendCreated(quint32 nodeid);
    void sendFailed(const QString &error);
    void sendClosed();

Q_SIGNALS:
    void finished();

private:
    friend class ScreencastV1InterfacePrivate;
    explicit ScreencastStreamV1Interface(QObject *parent);
    QScopedPointer<ScreencastStreamV1InterfacePrivate> d;
};

class KWAYLANDSERVER_EXPORT ScreencastV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit ScreencastV1Interface(Display *display, QObject *parent = nullptr);
    virtual ~ScreencastV1Interface();

    enum CursorMode {
        Hidden = 1,
        Embedded = 2,
        Metadata = 4,
    };
    Q_ENUM(CursorMode);

Q_SIGNALS:
    void outputScreencastRequested(ScreencastStreamV1Interface *stream, OutputInterface *output, CursorMode mode);
    void windowScreencastRequested(ScreencastStreamV1Interface *stream, const QString &winid, CursorMode mode);

private:
    QScopedPointer<ScreencastV1InterfacePrivate> d;
};

}
