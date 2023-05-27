/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinoffscreenquickview.h"
#include "libkwineffects/kwineffects.h"

class QAction;

namespace KWin
{
class GLTexture;

class TrackMouseEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
    Q_PROPERTY(bool mousePolling READ isMousePolling)
public:
    TrackMouseEffect();
    ~TrackMouseEffect() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime);
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen) override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;

    // for properties
    Qt::KeyboardModifiers modifiers() const
    {
        return m_modifiers;
    }
    bool isMousePolling() const
    {
        return m_mousePolling;
    }
private Q_SLOTS:
    void toggle();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    QAction *m_action;
    Qt::KeyboardModifiers m_modifiers;
    bool m_mousePolling = false;

    enum class State {
        ActivatedByModifiers,
        ActivatedByShortcut,
        Inactive
    };
    State m_state = State::Inactive;

    std::unique_ptr<OffscreenQuickScene> m_scene;
};

} // namespace
