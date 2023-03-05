/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class MouseMarkEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int width READ configuredWidth)
    Q_PROPERTY(QColor color READ configuredColor)
public:
    MouseMarkEffect();
    ~MouseMarkEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    // for properties
    int configuredWidth() const
    {
        return width;
    }
    QColor configuredColor() const
    {
        return color;
    }
private Q_SLOTS:
    void clear();
    void clearLast();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void screenLockingChanged(bool locked);

private:
    typedef QVector<QPointF> Mark;
    void drawMark(QPainter *painter, const Mark &mark);
    static Mark createArrow(QPointF arrow_start, QPointF arrow_end);
    QVector<Mark> marks;
    Mark drawing;
    QPointF arrow_start;
    int width;
    QColor color;
};

} // namespace
