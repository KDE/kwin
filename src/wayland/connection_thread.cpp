/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "connection_thread.h"
// Qt
#include <QDebug>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
// Wayland
#include <wayland-client-protocol.h>

namespace KWin
{

namespace Wayland
{

ConnectionThread::ConnectionThread(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_socketName(QString::fromUtf8(qgetenv("WAYLAND_DISPLAY")))
    , m_runtimeDir(QString::fromUtf8(qgetenv("XDG_RUNTIME_DIR")))
    , m_socketNotifier(nullptr)
    , m_socketWatcher(nullptr)
    , m_serverDied(false)
{
    if (m_socketName.isEmpty()) {
        m_socketName = QStringLiteral("wayland-0");
    }
}

ConnectionThread::~ConnectionThread()
{
    if (m_display) {
        wl_display_flush(m_display);
        wl_display_disconnect(m_display);
    }
}

void ConnectionThread::initConnection()
{
    QMetaObject::invokeMethod(this, "doInitConnection", Qt::QueuedConnection);
}

void ConnectionThread::doInitConnection()
{
    m_display = wl_display_connect(m_socketName.toUtf8().constData());
    if (!m_display) {
        qWarning() << "Failed connecting to Wayland display";
        emit failed();
        return;
    }
    qDebug() << "Connected to Wayland server at:" << m_socketName;

    // setup socket notifier
    setupSocketNotifier();
    setupSocketFileWatcher();
    emit connected();
}

void ConnectionThread::setupSocketNotifier()
{
    const int fd = wl_display_get_fd(m_display);
    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this,
        [this]() {
            if (!m_display) {
                return;
            }
            wl_display_dispatch(m_display);
            emit eventsRead();
        }
    );
}

void ConnectionThread::setupSocketFileWatcher()
{
    if (!m_runtimeDir.exists()) {
        return;
    }
    m_socketWatcher = new QFileSystemWatcher(this);
    m_socketWatcher->addPath(m_runtimeDir.absoluteFilePath(m_socketName));
    connect(m_socketWatcher, &QFileSystemWatcher::fileChanged, this,
        [this] (const QString &file) {
            if (QFile::exists(file) || m_serverDied) {
                return;
            }
            qWarning() << "Connection to server went away";
            m_serverDied = true;
            if (m_display) {
                free(m_display);
                m_display = nullptr;
            }
            delete m_socketNotifier;
            m_socketNotifier = nullptr;

            // need a new filesystem watcher
            delete m_socketWatcher;
            m_socketWatcher = new QFileSystemWatcher(this);
            m_socketWatcher->addPath(m_runtimeDir.absolutePath());
            connect(m_socketWatcher, &QFileSystemWatcher::directoryChanged, this,
                [this]() {
                    if (!m_serverDied) {
                        return;
                    }
                    if (m_runtimeDir.exists(m_socketName)) {
                        qDebug() << "Socket reappeared";
                        delete m_socketWatcher;
                        m_socketWatcher = nullptr;
                        m_serverDied = false;
                        initConnection();
                    }
                }
            );
            emit connectionDied();
        }
    );
}

void ConnectionThread::setSocketName(const QString &socketName)
{
    if (m_display) {
        // already initialized
        return;
    }
    m_socketName = socketName;
}

}
}
