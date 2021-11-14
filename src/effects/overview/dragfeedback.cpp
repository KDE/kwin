/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dragfeedback.h"

namespace KWin
{

Q_GLOBAL_STATIC(QList<DragMultiplexer *>, multiplexerList)

DragMultiplexer *DragMultiplexer::s_primary = nullptr;

DragMultiplexer::DragMultiplexer(QQuickItem *parent)
    : QQuickItem(parent)
{
    multiplexerList()->append(this);
}

DragMultiplexer::~DragMultiplexer()
{
    multiplexerList()->removeOne(this);
}

bool DragMultiplexer::isActive() const
{
    return !m_windowId.isEmpty();
}

QString DragMultiplexer::windowId() const
{
    return m_windowId;
}

QObject *DragMultiplexer::source() const
{
    return m_source;
}

QPointF DragMultiplexer::position() const
{
    return m_position;
}

void DragMultiplexer::setPosition(const QPointF &position)
{
    if (m_position == position) {
        return;
    }
    m_position = position;
    Q_EMIT positionChanged();

    // Notify others if it's the primary source.
    if (s_primary == this) {
        for (DragMultiplexer *feedback : *multiplexerList()) {
            if (feedback != this) {
                feedback->setPosition(position);
            }
        }
    }
}

void DragMultiplexer::start(const QString &windowId, QObject *source)
{
    if (!m_windowId.isEmpty()) {
        return;
    }
    if (!s_primary) {
        s_primary = this;
    }
    m_windowId = windowId;
    m_source = source;
    Q_EMIT windowIdChanged();
    Q_EMIT sourceChanged();

    // Notify others if it's the primary source.
    if (s_primary == this) {
        for (DragMultiplexer *feedback : *multiplexerList()) {
            if (feedback != this) {
                feedback->start(windowId, source);
            }
        }
    }
}

void DragMultiplexer::drop()
{
    if (m_windowId.isEmpty()) {
        return;
    }
    Q_EMIT dropped();
    m_windowId = QString();
    Q_EMIT windowIdChanged();

    // Notify others if it's the primary source.
    if (s_primary == this) {
        for (DragMultiplexer *feedback : *multiplexerList()) {
            if (feedback != this) {
                feedback->drop();
            }
        }
        s_primary = nullptr;
    }
}

} // namespace KWin
