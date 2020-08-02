/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_TRACKMOUSE_H
#define KWIN_TRACKMOUSE_H

#include <kwineffects.h>

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
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;

    // for properties
    Qt::KeyboardModifiers modifiers() const {
        return m_modifiers;
    }
    bool isMousePolling() const {
        return m_mousePolling;
    }
private Q_SLOTS:
    void toggle();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
private:
    bool init();
    void loadTexture();
    QRect m_lastRect[2];
    bool m_mousePolling;
    float m_angle;
    float m_angleBase;
    GLTexture* m_texture[2];
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    QSize m_size[2];
    XRenderPicture *m_picture[2];
#endif
    QAction* m_action;
    QImage m_image[2];
    Qt::KeyboardModifiers m_modifiers;

    enum class State {
        ActivatedByModifiers,
        ActivatedByShortcut,
        Inactive
    };
    State m_state = State::Inactive;
};

} // namespace

#endif
