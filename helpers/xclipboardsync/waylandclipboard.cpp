/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "waylandclipboard.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datasource.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/registry.h>

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QThread>
#include <QMimeData>
#include <QMimeType>
#include <qplatformdefs.h>

#include <unistd.h>

using namespace KWayland::Client;

WaylandClipboard::WaylandClipboard(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread)
    , m_connectionThread(new ConnectionThread)
{
    m_connectionThread->setSocketFd(qgetenv("WAYLAND_SOCKET").toInt());
    m_connectionThread->moveToThread(m_thread);
    m_thread->start();

    connect(m_connectionThread, &ConnectionThread::connected, this, &WaylandClipboard::setup, Qt::QueuedConnection);

    m_connectionThread->initConnection();

    connect(qApp->clipboard(), &QClipboard::changed, this,
        [this] (QClipboard::Mode mode) {
            if (mode != QClipboard::Clipboard) {
                return;
            }
            // TODO: do we need to take a copy of the clipboard in order to keep it after the X application quit?
            if (!m_dataDeviceManager || !m_dataDevice) {
                return;
            }
            auto source = m_dataDeviceManager->createDataSource(this);
            auto mimeData = qApp->clipboard()->mimeData();
            const auto formats = mimeData->formats();
            for (const auto &format : formats) {
                source->offer(format);
            }
            connect(source, &DataSource::sendDataRequested, this,
                [] (const QString &type, qint32 fd) {
                    auto mimeData = qApp->clipboard()->mimeData();
                    if (!mimeData->hasFormat(type)) {
                        close(fd);
                        return;
                    }
                    const auto data = mimeData->data(type);
                    QFile writePipe;
                    if (writePipe.open(fd, QIODevice::WriteOnly, QFile::AutoCloseHandle)) {
                        writePipe.write(data);
                        writePipe.close();
                    } else {
                        close(fd);
                    }
                }
            );
            m_dataDevice->setSelection(0, source);
            delete m_dataSource;
            m_dataSource = source;
            m_connectionThread->flush();
        }
    );
}

WaylandClipboard::~WaylandClipboard()
{
    m_connectionThread->deleteLater();
    m_thread->quit();
    m_thread->wait();
}

static int readData(int fd, QByteArray &data)
{
    // implementation based on QtWayland file qwaylanddataoffer.cpp
    char buf[4096];
    int retryCount = 0;
    int n;
    while (true) {
        n = QT_READ(fd, buf, sizeof buf);
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK) && ++retryCount < 1000) {
            usleep(1000);
        } else {
            break;
        }
    }
    if (n > 0) {
        data.append(buf, n);
        n = readData(fd, data);
    }
    return n;
}

void WaylandClipboard::setup()
{
    EventQueue *queue = new EventQueue(this);
    queue->setup(m_connectionThread);

    Registry *registry = new Registry(this);
    registry->setEventQueue(queue);
    registry->create(m_connectionThread);
    connect(registry, &Registry::interfacesAnnounced, this,
        [this, registry] {
            const auto seatInterface = registry->interface(Registry::Interface::Seat);
            if (seatInterface.name != 0) {
                m_seat = registry->createSeat(seatInterface.name, seatInterface.version, this);
            }
            const auto ddmInterface = registry->interface(Registry::Interface::DataDeviceManager);
            if (ddmInterface.name != 0) {
                m_dataDeviceManager = registry->createDataDeviceManager(ddmInterface.name, ddmInterface.version, this);
            }
            if (m_seat && m_dataDeviceManager) {
                m_dataDevice = m_dataDeviceManager->getDataDevice(m_seat, this);
                connect(m_dataDevice, &DataDevice::selectionOffered, this,
                    [this] (DataOffer *offer) {
                        if (offer->offeredMimeTypes().isEmpty()) {
                            return;
                        }
                        int pipeFds[2];
                        if (pipe(pipeFds) != 0) {
                            return;
                        }
                        const auto mimeType = offer->offeredMimeTypes().first();
                        offer->receive(mimeType, pipeFds[1]);
                        m_connectionThread->flush();
                        close(pipeFds[1]);
                        QByteArray content;
                        if (readData(pipeFds[0], content) != 0) {
                            content = QByteArray();
                        }
                        close(pipeFds[0]);
                        QMimeData *mimeData = new QMimeData();
                        mimeData->setData(mimeType.name(), content);
                        qApp->clipboard()->setMimeData(mimeData);
                    }
                );
                connect(m_dataDevice, &DataDevice::selectionCleared, this,
                    [this] {
                        qApp->clipboard()->clear();
                    }
                );
            }
        }
    );
    registry->setup();
}
