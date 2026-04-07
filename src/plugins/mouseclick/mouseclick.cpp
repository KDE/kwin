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
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "input_event.h"

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

    initOffscreenViews();

    // m_buttons[0] = std::make_unique<MouseButton>(i18nc("Left mouse button", "Left"), Qt::LeftButton);
    // m_buttons[1] = std::make_unique<MouseButton>(i18nc("Middle mouse button", "Middle"), Qt::MiddleButton);
    // m_buttons[2] = std::make_unique<MouseButton>(i18nc("Right mouse button", "Right"), Qt::RightButton);
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
    // const int time = m_clock.tick(data.view).count();

    // for (auto &click : m_clicks) {
    //     click->m_time += time;
    // }

    // for (int i = 0; i < BUTTON_COUNT; ++i) {
    //     if (m_buttons[i]->m_isPressed) {
    //         m_buttons[i]->m_time += time;
    //     }
    // }

    // while (m_clicks.size() > 0) {
    //     if (m_clicks.front()->m_time <= m_ringLife) {
    //         break;
    //     }
    //     m_clicks.pop_front();
    // }

    // if (!isActive()) {
    //     m_clock.reset();
    // }

    effects->prePaintScreen(data);
}

void MouseClickEffect::initOffscreenViews()
{
    // DAVE, I don't handle screen changes or size changes
    // but hopefully it's a short lived thing
    if (!m_scenesByScreens.empty()) {
        return;
    }
    const auto screens = effects->screens();
    for (const auto output : screens) {
        auto scene = new OffscreenQuickScene();

        scene->loadFromModule(QStringLiteral("org.kde.kwin.mouseclick"), QStringLiteral("Main"), {{QStringLiteral("effect"), QVariant::fromValue(this)}});
        scene->setGeometry(output->geometry());
        connect(scene, &OffscreenQuickView::repaintNeeded, this, [scene] {
            effects->addRepaint(scene->geometry());
        });
        m_scenesByScreens[output].reset(scene);
    }
}

void MouseClickEffect::cleanupOffscreenViews()
{
    m_scenesByScreens.clear();
}

void MouseClickEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    if (auto it = m_scenesByScreens.find(screen); it != m_scenesByScreens.end()) {
        effects->renderOffscreenQuickView(renderTarget, viewport, it->second.get());
    }
}

void MouseClickEffect::postPaintScreen()
{
    effects->postPaintScreen();
}

void MouseClickEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                        Qt::MouseButtons buttons, Qt::MouseButtons oldButtons,
                                        Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (buttons == oldButtons) {
        return;
    }

    // std::unique_ptr<MouseClickMouseEvent> m;
    // int i = BUTTON_COUNT;
    // while (--i >= 0) {
    //     MouseButton *b = m_buttons[i].get();
    //     if (isPressed(b->m_button, buttons, oldButtons)) {
    //         break;
    //     } else if (isReleased(b->m_button, buttons, oldButtons) && (!b->m_isPressed || b->m_time > m_ringLife)) {
    //         // we might miss a press, thus also check !b->m_isPressed, bug #314762
    //         break;
    //     }
    //     b->setPressed(b->m_button & buttons);
    // }

    // if (m) {
    //     m_clicks.push_back(std::move(m));
    // }
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

    // m_clicks.clear();
    m_tabletTools.clear();

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        // m_buttons[i]->m_time = 0;
        // m_buttons[i]->m_isPressed = false;
    }
}

bool MouseClickEffect::isActive() const
{
    return true;
    // return m_enabled && (m_clicks.size() != 0 || !m_tabletTools.isEmpty());
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
        m_tabletTools.remove(event->tool);
    }
    return false;
}

bool MouseClickEffect::tabletToolAxis(TabletToolAxisEvent *event)
{
    auto &point = getOrCreateTabletPoint(event->tool);
    point.m_globalPosition = event->position;
    point.m_pressure = event->pressure;
    return false;
}

bool MouseClickEffect::tabletToolTip(TabletToolTipEvent *event)
{
    auto &point = getOrCreateTabletPoint(event->tool);
    point.m_pressed = event->type == TabletToolTipEvent::Press;
    point.m_pressure = event->pressure;
    point.m_globalPosition = event->position;
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
