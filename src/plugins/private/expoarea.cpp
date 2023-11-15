/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "expoarea.h"
#include "virtualdesktops.h"
#include "workspace.h"

namespace KWin
{

ExpoArea::ExpoArea(QObject *parent)
    : QObject(parent)
{
}

qreal ExpoArea::x() const
{
    return m_rect.x();
}

qreal ExpoArea::y() const
{
    return m_rect.y();
}

qreal ExpoArea::width() const
{
    return m_rect.width();
}

qreal ExpoArea::height() const
{
    return m_rect.height();
}

Output *ExpoArea::screen() const
{
    return m_screen;
}

void ExpoArea::setScreen(Output *screen)
{
    if (m_screen != screen) {
        if (m_screen) {
            disconnect(m_screen, &Output::geometryChanged, this, &ExpoArea::update);
        }
        m_screen = screen;
        if (m_screen) {
            connect(m_screen, &Output::geometryChanged, this, &ExpoArea::update);
        }
        update();
        Q_EMIT screenChanged();
    }
}

void ExpoArea::update()
{
    if (!m_screen) {
        return;
    }
    const QRectF oldRect = m_rect;

    m_rect = workspace()->clientArea(MaximizeArea, m_screen, VirtualDesktopManager::self()->currentDesktop());

    // Map the area to the output local coordinates.
    m_rect.translate(-m_screen->geometry().topLeft());

    if (oldRect.x() != m_rect.x()) {
        Q_EMIT xChanged();
    }
    if (oldRect.y() != m_rect.y()) {
        Q_EMIT yChanged();
    }
    if (oldRect.width() != m_rect.width()) {
        Q_EMIT widthChanged();
    }
    if (oldRect.height() != m_rect.height()) {
        Q_EMIT heightChanged();
    }
}

} // namespace KWin

#include "moc_expoarea.cpp"
