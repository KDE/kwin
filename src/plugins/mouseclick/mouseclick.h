/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/effectframe.h"
#include <KLocalizedString>
#include <QFont>
#include <QHash>
#include <array>
#include <memory>

namespace KWin
{

#define BUTTON_COUNT 3

class GLFramebuffer;
class GLShader;
class GLTexture;

class MouseClickMouseEvent
{
public:
    MouseClickMouseEvent(int button, QPoint point, int time, std::unique_ptr<EffectFrame> &&frame, bool press)
        : m_button(button)
        , m_pos(point)
        , m_time(time)
        , m_frame(std::move(frame))
        , m_press(press)
    {
    }

    int m_button;
    QPoint m_pos;
    int m_time;
    std::unique_ptr<EffectFrame> m_frame;
    bool m_press;
};

class TabletToolEvent
{
public:
    QPointF m_globalPosition;
    bool m_pressed = false;
    qreal m_pressure = 0;
    QColor m_color;
};

class MouseButton
{
public:
    MouseButton(QString label, Qt::MouseButtons button)
        : m_labelUp(label)
        , m_labelDown(label)
        , m_button(button)
        , m_isPressed(false)
        , m_time(0)
    {
        m_labelDown.append(i18n("↓"));
        m_labelUp.append(i18n("↑"));
    }

    inline void setPressed(bool pressed)

    {
        if (m_isPressed != pressed) {
            m_isPressed = pressed;
            if (pressed) {
                m_time = 0;
            }
        }
    }

    QString m_labelUp;
    QString m_labelDown;
    Qt::MouseButtons m_button;
    bool m_isPressed;
    int m_time;
};

class MouseClickEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(QColor color1 READ color1)
    Q_PROPERTY(QColor color2 READ color2)
    Q_PROPERTY(QColor color3 READ color3)
    Q_PROPERTY(qreal lineWidth READ lineWidth)
    Q_PROPERTY(int ringLife READ ringLife)
    Q_PROPERTY(int ringSize READ ringSize)
    Q_PROPERTY(int ringCount READ ringCount)
    Q_PROPERTY(bool showText READ isShowText)
    Q_PROPERTY(QFont font READ font)
    Q_PROPERTY(bool enabled READ isEnabled)

public:
    MouseClickEffect();
    ~MouseClickEffect() override;
    void reconfigure(ReconfigureFlags) override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;

    // for properties
    QColor color1() const;
    QColor color2() const;
    QColor color3() const;
    qreal lineWidth() const;
    int ringLife() const;
    int ringSize() const;
    int ringCount() const;
    bool isShowText() const;
    QFont font() const;
    bool isEnabled() const;

private Q_SLOTS:
    void toggleEnabled();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    struct OffscreenTarget
    {
        std::shared_ptr<GLTexture> texture;
        std::shared_ptr<GLFramebuffer> framebuffer;
    };

    struct ScreenCapture
    {
        OffscreenTarget scene;
        std::array<OffscreenTarget, 2> feedback;
        int previousFrame = 0;
    };

    void loadShaders();

    std::unique_ptr<EffectFrame> createEffectFrame(const QPoint &pos, const QString &text);
    inline bool isReleased(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons);
    inline bool isPressed(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons);

    QColor m_colors[3];
    int m_ringCount;
    float m_lineWidth;
    float m_ringLife;
    float m_ringMaxSize;
    bool m_showText;
    QFont m_font;
    QHash<LogicalOutput *, ScreenCapture> m_screenCaptures;

    std::unique_ptr<MouseClickMouseEvent> m_click;
    std::unique_ptr<MouseButton> m_buttons[BUTTON_COUNT];

    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<GLShader> m_feedbackShader;
    int m_modelViewProjectionMatrixLocation = -1;
    int m_cursorColorLocation = -1;
    int m_cursorPositionLocation = -1;
    int m_cursorRadiusLocation = -1;
    int m_feedbackModelViewProjectionMatrixLocation = -1;
    int m_feedbackPreviousFrameLocation = -1;
    int m_feedbackScreenSizeLocation = -1;

    bool m_enabled;
};

} // namespace
