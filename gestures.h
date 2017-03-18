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
#ifndef KWIN_GESTURES_H
#define KWIN_GESTURES_H

#include <QObject>
#include <QMap>
#include <QVector>

namespace KWin
{

class Gesture : public QObject
{
    Q_OBJECT
public:
    ~Gesture() override;
protected:
    explicit Gesture(QObject *parent);

Q_SIGNALS:
    /**
     * Matching of a gesture started and this Gesture might match.
     * On further evaluation either the signal @link{triggered} or
     * @link{cancelled} will get emitted.
     **/
    void started();
    /**
     * Gesture matching ended and this Gesture matched.
     **/
    void triggered();
    /**
     * This Gesture no longer matches.
     **/
    void cancelled();
};

class SwipeGesture : public Gesture
{
    Q_OBJECT
public:
    enum class Direction {
        Down,
        Left,
        Up,
        Right
    };

    explicit SwipeGesture(QObject *parent = nullptr);
    ~SwipeGesture() override;

    bool minimumFingerCountIsRelevant() const {
        return m_minimumFingerCountRelevant;
    }
    void setMinimumFingerCount(uint count) {
        m_minimumFingerCount = count;
        m_minimumFingerCountRelevant = true;
    }
    uint minimumFingerCount() const {
        return m_minimumFingerCount;
    }

    bool maximumFingerCountIsRelevant() const {
        return m_maximumFingerCountRelevant;
    }
    void setMaximumFingerCount(uint count) {
        m_maximumFingerCount = count;
        m_maximumFingerCountRelevant = true;
    }
    uint maximumFingerCount() const {
        return m_maximumFingerCount;
    }

    Direction direction() const {
        return m_direction;
    }
    void setDirection(Direction direction) {
        m_direction = direction;
    }

private:
    bool m_minimumFingerCountRelevant = false;
    uint m_minimumFingerCount = 0;
    bool m_maximumFingerCountRelevant = false;
    uint m_maximumFingerCount = 0;
    Direction m_direction = Direction::Down;
};

class GestureRecognizer : public QObject
{
    Q_OBJECT
public:
    GestureRecognizer(QObject *parent = nullptr);
    ~GestureRecognizer() override;

    void registerGesture(Gesture *gesture);
    void unregisterGesture(Gesture *gesture);

    void startSwipeGesture(uint fingerCount);
    void updateSwipeGesture(const QSizeF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

private:
    void cancelActiveSwipeGestures();
    QVector<Gesture*> m_gestures;
    QVector<Gesture*> m_activeSwipeGestures;
    QMap<Gesture*, QMetaObject::Connection> m_destroyConnections;
    QVector<QSizeF> m_swipeUpdates;
};

}

Q_DECLARE_METATYPE(KWin::SwipeGesture::Direction)

#endif
