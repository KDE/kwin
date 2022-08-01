/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class OutputInterface;
class ScreencastV1InterfacePrivate;
class ScreencastStreamV1InterfacePrivate;
class ScreencastStreamV1Interface;

class KWIN_EXPORT ScreencastStreamV1Interface : public QObject
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
    std::unique_ptr<ScreencastStreamV1InterfacePrivate> d;
};

class KWIN_EXPORT ScreencastV1Interface : public QObject
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
    Q_ENUM(CursorMode)

Q_SIGNALS:
    void outputScreencastRequested(ScreencastStreamV1Interface *stream, OutputInterface *output, CursorMode mode);
    void virtualOutputScreencastRequested(ScreencastStreamV1Interface *stream, const QString &name, const QSize &size, double scaling, CursorMode mode);
    void windowScreencastRequested(ScreencastStreamV1Interface *stream, const QString &winid, CursorMode mode);
    void regionScreencastRequested(ScreencastStreamV1Interface *stream, const QRect &geometry, qreal scaling, CursorMode mode);

private:
    std::unique_ptr<ScreencastV1InterfacePrivate> d;
};

}
