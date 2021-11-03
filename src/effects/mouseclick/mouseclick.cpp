/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclick.h"
// KConfigSkeleton
#include "mouseclickconfig.h"

#include <QAction>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QPainter>

#include <cmath>

namespace KWin
{

MouseClickEffect::MouseClickEffect()
{
    initConfig<MouseClickConfig>();
    m_enabled = false;

    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("ToggleMouseClick"));
    a->setText(i18n("Toggle Mouse Click Effect"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Asterisk);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Asterisk);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Asterisk, a);
    connect(a, &QAction::triggered, this, &MouseClickEffect::toggleEnabled);

    reconfigure(ReconfigureAll);

    m_buttons[0] = new MouseButton(i18nc("Left mouse button", "Left"), Qt::LeftButton);
    m_buttons[1] = new MouseButton(i18nc("Middle mouse button", "Middle"), Qt::MiddleButton);
    m_buttons[2] = new MouseButton(i18nc("Right mouse button", "Right"), Qt::RightButton);
}

MouseClickEffect::~MouseClickEffect()
{
    if (m_enabled)
        effects->stopMousePolling();
    qDeleteAll(m_clicks);
    m_clicks.clear();

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        delete m_buttons[i];
    }
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

void MouseClickEffect::prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
    const int time = m_lastPresentTime.count() ? (presentTime - m_lastPresentTime).count() : 0;

    for (MouseEvent* click : qAsConst(m_clicks)) {
        click->m_time += time;
    }

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        if (m_buttons[i]->m_isPressed) {
            m_buttons[i]->m_time += time;
        }
    }

    while (m_clicks.size() > 0) {
        MouseEvent* first = m_clicks[0];
        if (first->m_time <= m_ringLife) {
            break;
        }
        m_clicks.pop_front();
        delete first;
    }

    if (isActive()) {
        m_lastPresentTime = presentTime;
    } else {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    }

    effects->prePaintScreen(data, presentTime);
}

void MouseClickEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);

    paintScreenSetup(mask, region, data);
    for (const MouseEvent *click : qAsConst(m_clicks)) {
        for (int i = 0; i < m_ringCount; ++i) {
            float alpha = computeAlpha(click, i);
            float size = computeRadius(click, i);
            if (size > 0 && alpha > 0) {
                QColor color = m_colors[click->m_button];
                color.setAlphaF(alpha);
                drawCircle(color, click->m_pos.x(), click->m_pos.y(), size);
            }
        }

        if (m_showText && click->m_frame) {
            float frameAlpha = (click->m_time * 2.0f - m_ringLife) / m_ringLife;
            frameAlpha = frameAlpha < 0 ? 1 : -(frameAlpha * frameAlpha) + 1;
            click->m_frame->render(infiniteRegion(), frameAlpha, frameAlpha);
        }
    }
    paintScreenFinish(mask, region, data);
}

void MouseClickEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

float MouseClickEffect::computeRadius(const MouseEvent* click, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    if (click->m_press) {
        return ((click->m_time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
    }
    return ((m_ringLife - click->m_time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
}

float MouseClickEffect::computeAlpha(const MouseEvent* click, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    return (m_ringLife - (float)click->m_time - ringDistance * (ring)) / m_ringLife;
}

void MouseClickEffect::slotMouseChanged(const QPoint& pos, const QPoint&,
                                        Qt::MouseButtons buttons, Qt::MouseButtons oldButtons,
                                        Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (buttons == oldButtons)
        return;

    MouseEvent* m = nullptr;
    int i = BUTTON_COUNT;
    while (--i >= 0) {
        MouseButton* b = m_buttons[i];
        if (isPressed(b->m_button, buttons, oldButtons)) {
            m = new MouseEvent(i, pos, 0, createEffectFrame(pos, b->m_labelDown), true);
            break;
        } else if (isReleased(b->m_button, buttons, oldButtons) && (!b->m_isPressed || b->m_time > m_ringLife)) {
            // we might miss a press, thus also check !b->m_isPressed, bug #314762
            m = new MouseEvent(i, pos, 0, createEffectFrame(pos, b->m_labelUp), false);
            break;
        }
        b->setPressed(b->m_button & buttons);
    }

    if (m) {
        m_clicks.append(m);
    }
    repaint();
}

EffectFrame* MouseClickEffect::createEffectFrame(const QPoint& pos, const QString& text) {
    if (!m_showText) {
        return nullptr;
    }
    QPoint point(pos.x() + m_ringMaxSize, pos.y());
    EffectFrame* frame = effects->effectFrame(EffectFrameStyled, false, point, Qt::AlignLeft);
    frame->setFont(m_font);
    frame->setText(text);
    return frame;
}

void MouseClickEffect::repaint()
{
    if (m_clicks.size() > 0) {
        QRegion dirtyRegion;
        const int radius = m_ringMaxSize + m_lineWidth;
        for (MouseEvent *click : qAsConst(m_clicks)) {
            dirtyRegion |= QRect(click->m_pos.x() - radius, click->m_pos.y() - radius, 2*radius, 2*radius);
            if (click->m_frame) {
                // we grant the plasma style 32px padding for stuff like shadows...
                dirtyRegion |= click->m_frame->geometry().adjusted(-32,-32,32,32);
            }
        }
        effects->addRepaint(dirtyRegion);
    }
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
        effects->startMousePolling();
    } else {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &MouseClickEffect::slotMouseChanged);
        effects->stopMousePolling();
    }

    qDeleteAll(m_clicks);
    m_clicks.clear();

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        m_buttons[i]->m_time = 0;
        m_buttons[i]->m_isPressed = false;
    }
}

bool MouseClickEffect::isActive() const
{
    return m_enabled && (m_clicks.size() > 0);
}

void MouseClickEffect::drawCircle(const QColor& color, float cx, float cy, float r)
{
    if (effects->isOpenGLCompositing())
        drawCircleGl(color, cx, cy, r);
    else if (effects->compositingType() == QPainterCompositing)
        drawCircleQPainter(color, cx, cy, r);
}

void MouseClickEffect::paintScreenSetup(int mask, QRegion region, ScreenPaintData& data)
{
    if (effects->isOpenGLCompositing())
        paintScreenSetupGl(mask, region, data);
}

void MouseClickEffect::paintScreenFinish(int mask, QRegion region, ScreenPaintData& data)
{
    if (effects->isOpenGLCompositing())
        paintScreenFinishGl(mask, region, data);
}

void MouseClickEffect::drawCircleGl(const QColor& color, float cx, float cy, float r)
{
    static const int num_segments = 80;
    static const float theta = 2 * 3.1415926 / float(num_segments);
    static const float c = cosf(theta); //precalculate the sine and cosine
    static const float s = sinf(theta);
    float t;

    float x = r;//we start at angle = 0
    float y = 0;

    GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setColor(color);
    QVector<float> verts;
    verts.reserve(num_segments * 2);

    for (int ii = 0; ii < num_segments; ++ii) {
        verts << x + cx << y + cy;//output vertex
        //apply the rotation matrix
        t = x;
        x = c * x - s * y;
        y = s * t + c * y;
    }
    vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
    vbo->render(GL_LINE_LOOP);
}

void MouseClickEffect::drawCircleQPainter(const QColor &color, float cx, float cy, float r)
{
    QPainter *painter = effects->scenePainter();
    painter->save();
    painter->setPen(color);
    painter->drawArc(cx - r, cy - r, r * 2, r * 2, 0, 5760);
    painter->restore();
}

void MouseClickEffect::paintScreenSetupGl(int, QRegion, ScreenPaintData &data)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::UniformColor);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());

    glLineWidth(m_lineWidth);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void MouseClickEffect::paintScreenFinishGl(int, QRegion, ScreenPaintData&)
{
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

} // namespace

