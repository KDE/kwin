/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "core/graphicsbuffer.h"
#include "wayland/graphicsbufferwatcher_p.h"

#include <QHash>

#include <poll.h>

using namespace KWin;

namespace KWaylandServer
{

GraphicsBufferWatcher *GraphicsBufferWatcher::get(GraphicsBuffer *buffer)
{
    static QHash<GraphicsBuffer *, GraphicsBufferWatcher *> watchers;
    if (auto it = watchers.find(buffer); it != watchers.end()) {
        return *it;
    }

    const DmaBufAttributes *attributes = buffer->dmabufAttributes();
    if (!attributes) {
        return nullptr;
    }

    auto watcher = new GraphicsBufferWatcher(attributes);
    watchers[buffer] = watcher;
    QObject::connect(buffer, &QObject::destroyed, [buffer]() {
        delete watchers.take(buffer);
    });

    return watcher;
}

static bool isReadable(int fileDescriptor)
{
    pollfd pfds[1];
    pfds[0].fd = fileDescriptor;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    if (poll(pfds, 1, 0) == -1) {
        return true;
    }

    return pfds->revents & POLLIN;
}

GraphicsBufferWatcher::GraphicsBufferWatcher(const DmaBufAttributes *attributes)
{
    for (int i = 0; i < attributes->planeCount; ++i) {
        auto notifier = new QSocketNotifier(attributes->fd[i].get(), QSocketNotifier::Read, this);
        notifier->setEnabled(false);
        connect(notifier, &QSocketNotifier::activated, this, [this, notifier]() {
            notifier->setEnabled(false);
            m_pending.removeOne(notifier);
            if (m_pending.isEmpty()) {
                Q_EMIT ready();
            }
        });
        m_notifiers << notifier;
    }
}

bool GraphicsBufferWatcher::arm()
{
    for (QSocketNotifier *notifier : std::as_const(m_notifiers)) {
        if (!isReadable(notifier->socket())) {
            notifier->setEnabled(true);
            m_pending.append(notifier);
        }
    }
    return !m_pending.isEmpty();
}

} // namespace KWaylandServer
