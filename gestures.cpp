/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#include "gestures.h"

#include <QSizeF>
#include <functional>

namespace KWin
{

Gesture::Gesture(QObject *parent)
    : QObject(parent)
{
}

Gesture::~Gesture() = default;

SwipeGesture::SwipeGesture(QObject *parent)
    : Gesture(parent)
{
}

SwipeGesture::~SwipeGesture() = default;

GestureRecognizer::GestureRecognizer(QObject *parent)
    : QObject(parent)
{
}

GestureRecognizer::~GestureRecognizer() = default;

void GestureRecognizer::registerGesture(KWin::Gesture* gesture)
{
    Q_ASSERT(!m_gestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_gestures << gesture;
}

void GestureRecognizer::unregisterGesture(KWin::Gesture* gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_gestures.removeAll(gesture);
    if (m_activeSwipeGestures.removeOne(gesture)) {
        emit gesture->cancelled();
    }
}

void GestureRecognizer::startSwipeGesture(uint fingerCount)
{
    // TODO: verify that no gesture is running
    for (Gesture *gesture : qAsConst(m_gestures)) {
        SwipeGesture *swipeGesture = qobject_cast<SwipeGesture*>(gesture);
        if (!gesture) {
            continue;
        }
        if (swipeGesture->minimumFingerCountIsRelevant()) {
            if (swipeGesture->minimumFingerCount() > fingerCount) {
                continue;
            }
        }
        if (swipeGesture->maximumFingerCountIsRelevant()) {
            if (swipeGesture->maximumFingerCount() < fingerCount) {
                continue;
            }
        }
        // direction doesn't matter yet
        m_activeSwipeGestures << swipeGesture;
        emit swipeGesture->started();
    }
}

void GestureRecognizer::updateSwipeGesture(const QSizeF &delta)
{
    m_swipeUpdates << delta;
    // determine the direction of the swipe
    if (delta.width() == delta.height()) {
        // special case of diagonal, this is not yet supported, thus cancel all gestures
        cancelActiveSwipeGestures();
        return;
    }
    SwipeGesture::Direction direction;
    if (std::abs(delta.width()) > std::abs(delta.height())) {
        // horizontal
        direction = delta.width() < 0 ? SwipeGesture::Direction::Left : SwipeGesture::Direction::Right;
    } else {
        // vertical
        direction = delta.height() < 0 ? SwipeGesture::Direction::Up : SwipeGesture::Direction::Down;
    }
    for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();) {
        auto g = qobject_cast<SwipeGesture*>(*it);
        if (g->direction() == direction) {
            it++;
        } else {
            emit g->cancelled();
            it = m_activeSwipeGestures.erase(it);
        }
    }
}

void GestureRecognizer::cancelActiveSwipeGestures()
{
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        emit g->cancelled();
    }
    m_activeSwipeGestures.clear();
}

void GestureRecognizer::cancelSwipeGesture()
{
    cancelActiveSwipeGestures();
    m_swipeUpdates.clear();
}

void GestureRecognizer::endSwipeGesture()
{
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        emit g->triggered();
    }
    m_activeSwipeGestures.clear();
    m_swipeUpdates.clear();
}

}
