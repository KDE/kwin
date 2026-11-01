/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2023 Andrew Shark <ashark at linuxcomp.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"

#include <QMap>
#include <QSet>

#include <map>
#include <memory>
#include <vector>

namespace KWin
{

class Item;
class MarkItem;

class MouseMarkEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int width READ configuredWidth)
    Q_PROPERTY(QColor color READ configuredColor)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ freedraw_modifiers)

    enum class State {
        NONE,
        FREEHAND,
        ARROW,
    };

public:
    MouseMarkEffect();
    ~MouseMarkEffect() override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;

    // for properties
    int configuredWidth() const
    {
        return width;
    }
    QColor configuredColor() const
    {
        return color;
    }
    Qt::KeyboardModifiers freedraw_modifiers() const
    {
        return m_freedraw_modifiers;
    }
private Q_SLOTS:
    void clear();
    void clearLast();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void screenLockingChanged(bool locked);

private:
    typedef QList<QPointF> Mark;
    static Mark createArrow(QPointF arrow_head, QPointF arrow_tail);

    std::unique_ptr<MarkItem> createMarkItem(const Mark &mark) const;
    void setState(State newState);
    void processPoint(qint32 channel, const QPointF &point); // ch0 is mouse, ch1+ is touch
    void endDraw(qint32 channel);
    void endDrawings();

    QList<Mark> marks; // marks on screen
    QMap<qint32, Mark> drawings; // marks currently being drawn
    // The parent of all the mark items, hidden while the screen is locked
    std::unique_ptr<Item> rootItem;
    // The items that show the marks, in the order of marks, and the items of drawings, by channel
    std::vector<std::unique_ptr<MarkItem>> markItems;
    std::map<qint32, std::unique_ptr<MarkItem>> drawingItems;
    State state;

    QSet<qint32> touchPoints; // touch points currently being consumed
                              // we need this so if state switches to NONE mid-touch-draw,
                              // the *entire* touch sequence is consumed, and none of it is leaked
                              // until touch release

    int width;
    QColor color;
    Qt::KeyboardModifiers m_freedraw_modifiers;
    Qt::KeyboardModifiers m_arrowdraw_modifiers;
};

} // namespace
