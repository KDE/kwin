/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>
#include <QFont>
#include <QHash>
#include <deque>
#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

#define BUTTON_COUNT 3

class MouseEvent
{
public:
    MouseEvent(int button, QPoint point, int time, std::unique_ptr<EffectFrame> &&frame, bool press)
        : m_button(button)
        , m_pos(point)
        , m_time(time)
        , m_frame(std::move(frame))
        , m_press(press){};

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
    };

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
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
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

    bool tabletToolEvent(QTabletEvent *event) override;

private Q_SLOTS:
    void toggleEnabled();
    void slotMouseChanged(const QPoint &pos, const QPoint &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    std::unique_ptr<EffectFrame> createEffectFrame(const QPoint &pos, const QString &text);
    inline void drawCircle(const QColor &color, float cx, float cy, float r);
    inline void paintScreenSetup(int mask, QRegion region, ScreenPaintData &data);
    inline void paintScreenFinish(int mask, QRegion region, ScreenPaintData &data);

    inline bool isReleased(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons);
    inline bool isPressed(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons);

    inline float computeRadius(const MouseEvent *click, int ring);
    inline float computeAlpha(const MouseEvent *click, int ring);

    void repaint();

    void drawCircleGl(const QColor &color, float cx, float cy, float r);
    void drawCircleQPainter(const QColor &color, float cx, float cy, float r);
    void paintScreenSetupGl(int mask, QRegion region, ScreenPaintData &data);
    void paintScreenFinishGl(int mask, QRegion region, ScreenPaintData &data);

    QColor m_colors[BUTTON_COUNT];
    int m_ringCount;
    float m_lineWidth;
    float m_ringLife;
    float m_ringMaxSize;
    bool m_showText;
    QFont m_font;
    std::chrono::milliseconds m_lastPresentTime = std::chrono::milliseconds::zero();

    std::deque<std::unique_ptr<MouseEvent>> m_clicks;
    std::unique_ptr<MouseButton> m_buttons[BUTTON_COUNT];
    QHash<quint64, TabletToolEvent> m_tabletTools;

    bool m_enabled;
};

} // namespace
