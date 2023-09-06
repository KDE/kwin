/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>
#include <QSocketNotifier>

namespace KWin
{
struct DmaBufAttributes;
class GraphicsBuffer;
}

namespace KWaylandServer
{

/**
 * The GraphicsBufferWatcher is a helper that provides a way to get notified when the associated
 * graphics buffer is ready to be used.
 */
class GraphicsBufferWatcher : public QObject
{
    Q_OBJECT

public:
    static GraphicsBufferWatcher *get(KWin::GraphicsBuffer *buffer);

public Q_SLOTS:
    bool arm();

Q_SIGNALS:
    void ready();

private:
    explicit GraphicsBufferWatcher(const KWin::DmaBufAttributes *attributes);

    QVector<QSocketNotifier *> m_pending;
    QVector<QSocketNotifier *> m_notifiers;
};

} // namespace KWaylandServer
