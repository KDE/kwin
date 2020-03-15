/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef TESTSERVER_H
#define TESTSERVER_H

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QVector>

class QElapsedTimer;
class QTimer;

namespace KWayland
{
namespace Server
{
class Display;
class SeatInterface;
class ShellInterface;
class ShellSurfaceInterface;
}
}

class TestServer : public QObject
{
    Q_OBJECT
public:
    explicit TestServer(QObject *parent);
    virtual ~TestServer();

    void init();
    void startTestApp(const QString &app, const QStringList &arguments);

private:
    void repaint();

    KWayland::Server::Display *m_display = nullptr;
    KWayland::Server::ShellInterface *m_shell = nullptr;
    KWayland::Server::SeatInterface *m_seat = nullptr;
    QVector<KWayland::Server::ShellSurfaceInterface*> m_shellSurfaces;
    QTimer *m_repaintTimer;
    QScopedPointer<QElapsedTimer> m_timeSinceStart;
    QPointF m_cursorPos;
    QHash<qint32, qint32> m_touchIdMapper;
};

#endif
