/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "scene/item.h"

namespace KWin
{

class ImageItem;

class RotatingArcsItem : public Item
{
    Q_OBJECT

public:
    explicit RotatingArcsItem(Item *parentItem);

    void rotate(qreal angle);

private:
    std::unique_ptr<ImageItem> m_outerArcItem;
    std::unique_ptr<ImageItem> m_innerArcItem;
};

class TrackMouseEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
public:
    TrackMouseEffect();
    ~TrackMouseEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;

    // for properties
    Qt::KeyboardModifiers modifiers() const
    {
        return m_modifiers;
    }
private Q_SLOTS:
    void toggle();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    enum class State {
        ActivatedByModifiers,
        ActivatedByShortcut,
        Inactive
    };

    bool needMouseEvents() const;

    State m_state = State::Inactive;
    float m_angle = 0;
    Qt::KeyboardModifiers m_modifiers;
    std::unique_ptr<RotatingArcsItem> m_rotatingArcsItem;
};

} // namespace
