/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/qpa/clipboard.h"
#include "utils/filedescriptor.h"
#include "utils/pipe.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland_server.h"

#include <QThreadPool>

#include <fcntl.h>
#include <poll.h>

namespace KWin::QPA
{

ClipboardDataSource::ClipboardDataSource(QMimeData *mimeData, QObject *parent)
    : AbstractDataSource(parent)
    , m_mimeData(mimeData)
{
}

QMimeData *ClipboardDataSource::mimeData() const
{
    return m_mimeData;
}

static void writeData(FileDescriptor fd, const QByteArray &buffer)
{
    size_t remainingSize = buffer.size();

    pollfd pfds[1];
    pfds[0].fd = fd.get();
    pfds[0].events = POLLOUT;

    while (true) {
        const int ready = poll(pfds, 1, 5000);
        if (ready < 0) {
            if (errno != EINTR) {
                return;
            }
        } else if (ready == 0) {
            return;
        } else if (!(pfds[0].revents & POLLOUT)) {
            return;
        } else {
            const char *chunk = buffer.constData() + (buffer.size() - remainingSize);
            const ssize_t n = write(fd.get(), chunk, remainingSize);

            if (n < 0) {
                return;
            } else if (n == 0) {
                return;
            } else {
                remainingSize -= n;
                if (remainingSize == 0) {
                    return;
                }
            }
        }
    }
}

void ClipboardDataSource::requestData(const QString &mimeType, FileDescriptor fd)
{
    const QByteArray data = m_mimeData->data(mimeType);
    QThreadPool::globalInstance()->start([data, pipe = std::move(fd)]() mutable {
        writeData(std::move(pipe), data);
    });
}

void ClipboardDataSource::cancel()
{
}

QStringList ClipboardDataSource::mimeTypes() const
{
    return m_mimeData->formats();
}

ClipboardMimeData::ClipboardMimeData(AbstractDataSource *dataSource)
    : m_dataSource(dataSource)
{
}

static QVariant readData(FileDescriptor fd)
{
    pollfd pfd;
    pfd.fd = fd.get();
    pfd.events = POLLIN;

    QByteArray buffer;
    while (true) {
        const int ready = poll(&pfd, 1, 1000);
        if (ready < 0) {
            if (errno != EINTR) {
                return QVariant();
            }
        } else if (ready == 0) {
            return QVariant();
        } else {
            char chunk[4096];
            const ssize_t n = read(fd.get(), chunk, sizeof chunk);

            if (n < 0) {
                return QVariant();
            } else if (n == 0) {
                return buffer;
            } else if (n > 0) {
                buffer.append(chunk, n);
            }
        }
    }
}

QVariant ClipboardMimeData::retrieveData(const QString &mimeType, QMetaType preferredType) const
{
    std::optional<Pipe> pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        return QVariant();
    }

    m_dataSource->requestData(mimeType, std::move(pipe->writeEndpoint));

    waylandServer()->display()->flush();
    return readData(std::move(pipe->readEndpoint));
}

Clipboard::Clipboard()
{
}

void Clipboard::initialize()
{
    connect(waylandServer()->seat(), &SeatInterface::selectionChanged, this, [this](AbstractDataSource *selection) {
        if (selection && m_ownSelection.get() != selection) {
            m_externalMimeData = std::make_unique<ClipboardMimeData>(selection);
        } else {
            m_externalMimeData.reset();
        }
        emitChanged(QClipboard::Clipboard);
    });
}

QMimeData *Clipboard::mimeData(QClipboard::Mode mode)
{
    switch (mode) {
    case QClipboard::Clipboard:
        if (m_ownSelection) {
            if (waylandServer()->seat()->selection() == m_ownSelection.get()) {
                return m_ownSelection->mimeData();
            }
        }
        return m_externalMimeData ? m_externalMimeData.get() : &m_emptyData;
    default:
        return &m_emptyData;
    }
}

void Clipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    static const QString plain = QStringLiteral("text/plain");
    static const QString utf8 = QStringLiteral("text/plain;charset=utf-8");

    if (data && data->hasFormat(plain) && !data->hasFormat(utf8)) {
        data->setData(utf8, data->data(plain));
    }

    switch (mode) {
    case QClipboard::Clipboard:
        if (data) {
            auto oldSelection = std::move(m_ownSelection);
            m_ownSelection = std::make_unique<ClipboardDataSource>(data);
            waylandServer()->seat()->setSelection(m_ownSelection.get(), waylandServer()->display()->nextSerial());
        } else {
            if (waylandServer()->seat()->selection() == m_ownSelection.get()) {
                waylandServer()->seat()->setSelection(nullptr, waylandServer()->display()->nextSerial());
            }
            m_ownSelection.reset();
        }
        break;
    default:
        break;
    }
}

bool Clipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard;
}

bool Clipboard::ownsMode(QClipboard::Mode mode) const
{
    switch (mode) {
    case QClipboard::Clipboard:
        return m_ownSelection && waylandServer()->seat()->selection() == m_ownSelection.get();
    default:
        return false;
    }
}

}
