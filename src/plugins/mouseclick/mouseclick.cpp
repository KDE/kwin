/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclick.h"
// KConfigSkeleton
#include "mouseclickconfig.h"

#include "core/inputdevice.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "scene/outlinedborderitem.h"
#include "scene/workspacescene.h"

#include <QAction>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <cmath>

namespace KWin
{

MouseClickEffect::MouseClickEffect()
{
    MouseClickConfig::instance(effects->config());
    m_enabled = false;

    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ToggleMouseClick"));
    a->setText(i18n("Toggle Mouse Click Animation"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::META | Qt::Key_Asterisk));
    connect(a, &QAction::triggered, this, &MouseClickEffect::toggleEnabled);

    reconfigure(ReconfigureAll);

    m_buttons[0] = std::make_unique<MouseButton>(i18nc("Left mouse button", "Left"), Qt::LeftButton);
    m_buttons[1] = std::make_unique<MouseButton>(i18nc("Middle mouse button", "Middle"), Qt::MiddleButton);
    m_buttons[2] = std::make_unique<MouseButton>(i18nc("Right mouse button", "Right"), Qt::RightButton);
}

MouseClickEffect::~MouseClickEffect()
{
}

void MouseClickEffect::reconfigure(ReconfigureFlags)
{
    MouseClickConfig::self()->read();
    m_colors[0] = MouseClickConfig::color1();
    m_colors[1] = MouseClickConfig::color2();
    m_colors[2] = MouseClickConfig::color3();
    m_lineWidth = MouseClickConfig::lineWidth();
    m_ringLife = MouseClickConfig::ringLife();
    m_ringMaxSize = MouseClickConfig::ringSize();
    m_ringCount = MouseClickConfig::ringCount();
    m_showText = MouseClickConfig::showText();
    m_font = MouseClickConfig::font();
}

void MouseClickEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const int time = m_clock.tickMs(data.view).count();

    for (auto &click : m_clicks) {
        click->m_time += time;
    }

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        if (m_buttons[i]->m_isPressed) {
            m_buttons[i]->m_time += time;
        }
    }

    while (m_clicks.size() > 0) {
        if (m_clicks.front()->m_time <= m_ringLife) {
            break;
        }
        m_clicks.pop_front();
    }
    for (const auto &click : m_clicks) {
        updateClick(click.get());
    }

    if (!isActive()) {
        m_clock.reset();
    }

    effects->prePaintScreen(data);
}

void MouseClickEffect::updateClick(MouseClickMouseEvent *click)
{
    for (int i = 0; i < m_ringCount; ++i) {
        const float alpha = computeAlpha(click, i);
        const float radius = computeRadius(click, i);
        OutlinedBorderItem *ring = click->m_rings[i].get();
        if (radius <= 0 || alpha <= 0) {
            ring->setVisible(false);
            continue;
        }
        setRing(ring, click->m_pos, radius, m_colors[click->m_button]);
        ring->setOpacity(alpha);
    }

    if (m_showText && click->m_frame) {
        float frameAlpha = (click->m_time * 2.0f - m_ringLife) / m_ringLife;
        frameAlpha = frameAlpha < 0 ? 1 : -(frameAlpha * frameAlpha) + 1;
        click->m_frame->show(frameAlpha, frameAlpha);
    }
}

void MouseClickEffect::updateTabletTool(TabletToolEvent &tool)
{
    // One ring at the full size, then one more for each step, so a harder press draws more rings
    const int step = m_ringMaxSize * (1. - tool.m_pressure);
    const size_t count = step > 0 ? std::ceil(m_ringMaxSize / step) : 1;
    while (tool.m_rings.size() > count) {
        tool.m_rings.pop_back();
    }
    while (tool.m_rings.size() < count) {
        tool.m_rings.push_back(std::make_unique<OutlinedBorderItem>(RectF(), BorderOutline(), effects->scene()->overlayItem()));
    }
    for (size_t i = 0; i < count; ++i) {
        setRing(tool.m_rings[i].get(), tool.m_globalPosition, m_ringMaxSize - i * step, tool.m_color);
    }
}

void MouseClickEffect::setRing(OutlinedBorderItem *ring, const QPointF &center, float radius, const QColor &color)
{
    if (radius <= 0) {
        ring->setVisible(false);
        return;
    }
    LogicalOutput *output = effects->screenAt(center.toPoint());
    // The line width is in device pixels
    const qreal lineWidth = m_lineWidth / (output ? output->scale() : 1.0);
    // The line is centered on the circle of this radius. A circle smaller than the line is drawn
    // as a dot, with an inner radius just above zero.
    const qreal outerRadius = radius + lineWidth / 2;
    const qreal innerRadius = std::max<qreal>(radius - lineWidth / 2, 0.01);
    ring->setOutline(BorderOutline(outerRadius - innerRadius, color, BorderRadius(innerRadius)));
    ring->setInnerRect(RectF(center.x() - innerRadius, center.y() - innerRadius, 2 * innerRadius, 2 * innerRadius));
    ring->setVisible(true);
}

void MouseClickEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

void MouseClickEffect::repaint()
{
    // The rings of a press start with a radius of 0 and are hidden, so the area is repainted to
    // get the next frame
    Region dirtyRegion;
    const int radius = m_ringMaxSize + m_lineWidth;
    for (const auto &click : m_clicks) {
        dirtyRegion += Rect(click->m_pos.x() - radius, click->m_pos.y() - radius, 2 * radius, 2 * radius);
    }
    if (!dirtyRegion.isEmpty()) {
        effects->addRepaint(dirtyRegion);
    }
}

float MouseClickEffect::computeRadius(const MouseClickMouseEvent *click, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    if (click->m_press) {
        return ((click->m_time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
    }
    return ((m_ringLife - click->m_time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
}

float MouseClickEffect::computeAlpha(const MouseClickMouseEvent *click, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    return (m_ringLife - (float)click->m_time - ringDistance * (ring)) / m_ringLife;
}

void MouseClickEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                        Qt::MouseButtons buttons, Qt::MouseButtons oldButtons,
                                        Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (buttons == oldButtons) {
        return;
    }

    std::unique_ptr<MouseClickMouseEvent> m;
    int i = BUTTON_COUNT;
    while (--i >= 0) {
        MouseButton *b = m_buttons[i].get();
        if (isPressed(b->m_button, buttons, oldButtons)) {
            m = std::make_unique<MouseClickMouseEvent>(i, pos.toPoint(), 0, createEffectFrame(pos.toPoint(), b->m_labelDown), true);
            break;
        } else if (isReleased(b->m_button, buttons, oldButtons) && (!b->m_isPressed || b->m_time > m_ringLife)) {
            // we might miss a press, thus also check !b->m_isPressed, bug #314762
            m = std::make_unique<MouseClickMouseEvent>(i, pos.toPoint(), 0, createEffectFrame(pos.toPoint(), b->m_labelUp), false);
            break;
        }
        b->setPressed(b->m_button & buttons);
    }

    if (m) {
        for (int ring = 0; ring < m_ringCount; ++ring) {
            m->m_rings.push_back(std::make_unique<OutlinedBorderItem>(RectF(), BorderOutline(), effects->scene()->overlayItem()));
        }
        updateClick(m.get());
        m_clicks.push_back(std::move(m));
    }
    repaint();
}

std::unique_ptr<EffectFrame> MouseClickEffect::createEffectFrame(const QPoint &pos, const QString &text)
{
    if (!m_showText) {
        return nullptr;
    }
    QPoint point(pos.x() + m_ringMaxSize, pos.y());
    std::unique_ptr<EffectFrame> frame = std::make_unique<EffectFrame>(EffectFrameStyled, false, point, Qt::AlignLeft);
    frame->setFont(m_font);
    frame->setText(text);
    return frame;
}

bool MouseClickEffect::isReleased(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons)
{
    return !(button & buttons) && (button & oldButtons);
}

bool MouseClickEffect::isPressed(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons)
{
    return (button & buttons) && !(button & oldButtons);
}

void MouseClickEffect::toggleEnabled()
{
    m_enabled = !m_enabled;

    if (m_enabled) {
        connect(effects, &EffectsHandler::mouseChanged, this, &MouseClickEffect::slotMouseChanged);
    } else {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &MouseClickEffect::slotMouseChanged);
    }

    m_clicks.clear();
    m_tabletTools.clear();

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        m_buttons[i]->m_time = 0;
        m_buttons[i]->m_isPressed = false;
    }
}

bool MouseClickEffect::isActive() const
{
    return m_enabled && (m_clicks.size() != 0 || !m_tabletTools.empty());
}

TabletToolEvent &MouseClickEffect::getOrCreateTabletPoint(InputDeviceTabletTool *tool)
{
    auto &point = m_tabletTools[tool];
    if (!point.m_color.isValid()) {
        switch (tool->type()) {
        case InputDeviceTabletTool::Type::Finger:
        case InputDeviceTabletTool::Type::Pen:
        case InputDeviceTabletTool::Pencil:
        case InputDeviceTabletTool::Brush:
        case InputDeviceTabletTool::Airbrush:
        case InputDeviceTabletTool::Lens:
        case InputDeviceTabletTool::Totem:
            point.m_color = MouseClickConfig::color1();
            break;
        case InputDeviceTabletTool::Type::Eraser:
            point.m_color = MouseClickConfig::color2();
            break;
        case KWin::InputDeviceTabletTool::Type::Mouse:
            point.m_color = MouseClickConfig::color3();
            break;
            break;
        }
    }
    return point;
}

bool MouseClickEffect::tabletToolProximity(TabletToolProximityEvent *event)
{
    if (event->type == TabletToolProximityEvent::LeaveProximity) {
        m_tabletTools.erase(event->tool);
    }
    return false;
}

bool MouseClickEffect::tabletToolAxis(TabletToolAxisEvent *event)
{
    if (!m_enabled) {
        return false;
    }
    auto &point = getOrCreateTabletPoint(event->tool);
    point.m_globalPosition = event->position;
    point.m_pressure = event->pressure;
    updateTabletTool(point);
    return false;
}

bool MouseClickEffect::tabletToolTip(TabletToolTipEvent *event)
{
    if (!m_enabled) {
        return false;
    }
    auto &point = getOrCreateTabletPoint(event->tool);
    point.m_pressed = event->type == TabletToolTipEvent::Press;
    point.m_pressure = event->pressure;
    point.m_globalPosition = event->position;
    updateTabletTool(point);
    return false;
}

QColor MouseClickEffect::color1() const
{
    return m_colors[0];
}

QColor MouseClickEffect::color2() const
{
    return m_colors[1];
}

QColor MouseClickEffect::color3() const
{
    return m_colors[2];
}

qreal MouseClickEffect::lineWidth() const
{
    return m_lineWidth;
}

int MouseClickEffect::ringLife() const
{
    return m_ringLife;
}

int MouseClickEffect::ringSize() const
{
    return m_ringMaxSize;
}

int MouseClickEffect::ringCount() const
{
    return m_ringCount;
}

bool MouseClickEffect::isShowText() const
{
    return m_showText;
}

QFont MouseClickEffect::font() const
{
    return m_font;
}

bool MouseClickEffect::isEnabled() const
{
    return m_enabled;
}

} // namespace

#include "moc_mouseclick.cpp"
