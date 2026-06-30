/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2023 Andrew Shark <ashark at linuxcomp.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mousemark.h"
#include "mousemarklogging.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glplatform.h"
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>

#include <cmath>

namespace KWin
{

static consteval QPoint nullPoint()
{
    return QPoint(-1, -1);
}

MouseMarkEffect::MouseMarkEffect()
{
    MouseMarkConfig::instance(effects->config());
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear All Mouse Marks"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_F11));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clear);
    a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_F12));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clearLast);

    connect(effects, &EffectsHandler::mouseChanged, this, &MouseMarkEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &MouseMarkEffect::screenLockingChanged);
    reconfigure(ReconfigureAll);
}

MouseMarkEffect::~MouseMarkEffect()
{
}

static int width_2 = 1;
void MouseMarkEffect::reconfigure(ReconfigureFlags)
{
    m_freedraw_modifiers = Qt::KeyboardModifiers();
    m_arrowdraw_modifiers = Qt::KeyboardModifiers();
    MouseMarkConfig::self()->read();
    width = MouseMarkConfig::lineWidth();
    width_2 = width / 2;
    color = MouseMarkConfig::color();
    color.setAlphaF(1.0);
    if (MouseMarkConfig::freedrawshift()) {
        m_freedraw_modifiers |= Qt::ShiftModifier;
    }
    if (MouseMarkConfig::freedrawalt()) {
        m_freedraw_modifiers |= Qt::AltModifier;
    }
    if (MouseMarkConfig::freedrawcontrol()) {
        m_freedraw_modifiers |= Qt::ControlModifier;
    }
    if (MouseMarkConfig::freedrawmeta()) {
        m_freedraw_modifiers |= Qt::MetaModifier;
    }

    if (MouseMarkConfig::arrowdrawshift()) {
        m_arrowdraw_modifiers |= Qt::ShiftModifier;
    }
    if (MouseMarkConfig::arrowdrawalt()) {
        m_arrowdraw_modifiers |= Qt::AltModifier;
    }
    if (MouseMarkConfig::arrowdrawcontrol()) {
        m_arrowdraw_modifiers |= Qt::ControlModifier;
    }
    if (MouseMarkConfig::arrowdrawmeta()) {
        m_arrowdraw_modifiers |= Qt::MetaModifier;
    }
}

void MouseMarkEffect::setState(State newState)
{
    if (state == newState) {
        return;
    }

    state = newState;
    switch (state) {
    case State::NONE:
        // flush all
        endDrawings();
        break;
    case State::ARROW:
    case State::FREEHAND:
        // Flush drawings, and continue new marks from there
        for (Mark &mark : drawings) {
            if (mark.size() >= 2) {
                marks.append(mark);
                mark = {mark.last()};
            }
        }
        break;
    }
}

void MouseMarkEffect::processPoint(qint32 id, const QPointF &pos)
{
    if (state == State::NONE) {
        return;
    }
    Mark &drawing = drawings[id];
    switch (state) {
    case State::NONE:
        return;
    case State::FREEHAND: {
        if (drawing.isEmpty()) {
            drawing.append(pos);
        }
        if (drawing.last() == pos) {
            return;
        }
        QPointF pos2 = drawing.last();
        drawing.append(pos);
        Rect repaint = Rect(std::min(pos.x(), pos2.x()), std::min(pos.y(), pos2.y()),
                            std::max(pos.x(), pos2.x()), std::max(pos.y(), pos2.y()));
        repaint.adjust(-width, -width, width, width);
        effects->addRepaint(repaint);
        break;
    }
    case State::ARROW: {
        if (drawing.size() < 1) {
            // New arrow
            drawing.append(pos);
        } else if (drawing.back() != pos) {
            // update existing arrow
            QPointF tail = drawing.front();
            drawing = createArrow(pos, tail);
            effects->addRepaintFull();
        }
        break;
    }
    }
}

void MouseMarkEffect::endDraw(qint32 channel)
{
    const auto it = drawings.find(channel);
    if (it == drawings.end()) {
        return;
    }

    if (it->size() >= 2) {
        marks.append(std::move(*it));
        effects->addRepaintFull();
    }

    drawings.erase(it);
}

void MouseMarkEffect::endDrawings()
{
    for (Mark &drawing : drawings) {
        if (drawing.size() >= 2) {
            marks.append(std::move(drawing));
        }
    }
    drawings.clear();
    effects->addRepaintFull();
}

void MouseMarkEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen); // paint normal screen
    if (marks.isEmpty() && drawings.isEmpty()) {
        return;
    }
    if (effects->openglContext()) {
        glLineWidth(width);
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        const auto scale = viewport.scale();
        ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
        binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
        binder.shader()->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
        binder.shader()->setUniform(GLShader::ColorUniform::Color, color);
        QList<QVector2D> verts;
        for (const Mark &mark : std::as_const(marks)) {
            verts.clear();
            verts.reserve(mark.size());
            for (const QPointF &p : std::as_const(mark)) {
                verts.push_back(QVector2D(p.x() * scale, p.y() * scale));
            }
            vbo->setVertices(verts);
            vbo->render(GL_LINE_STRIP);
        }
        for (const Mark &drawing : std::as_const(drawings)) {
            if (!drawing.isEmpty()) {
                verts.clear();
                verts.reserve(drawing.size());
                for (const QPointF &p : std::as_const(drawing)) {
                    verts.push_back(QVector2D(p.x() * scale, p.y() * scale));
                }
                vbo->setVertices(verts);
                vbo->render(GL_LINE_STRIP);
            }
        }
        glLineWidth(1.0);
    }
}

bool MouseMarkEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchDown id=" << id << " pos=" << pos;
    if (state == State::NONE) {
        return false;
    }
    if (touchPoints.contains(id)) {
        // Will this happen?
        qCWarning(KWIN_MOUSEMARK) << "WARNING: Touch started twice! " << __FILE__ << __LINE__;
        return true;
    }
    touchPoints.insert(id);
    processPoint(id + 1, pos);
    return true;
}

bool MouseMarkEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchMotion id=" << id << " pos=" << pos;
    if (state == State::NONE) {
        if (touchPoints.contains(id)) {
            return true;
        }
        return false;
    }
    processPoint(id + 1, pos);
    return true;
}

bool MouseMarkEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    qCDebug(KWIN_MOUSEMARK) << "touchUp id=" << id;
    if (state == State::NONE) {
        if (touchPoints.contains(id)) {
            touchPoints.remove(id);
            return true;
        }
        return false;
    }
    endDraw(id + 1);
    if (!touchPoints.contains(id)) {
        // touch began before drawing activation
        return false;
    }
    touchPoints.remove(id);
    return true;
}

void MouseMarkEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                       Qt::MouseButtons, Qt::MouseButtons,
                                       Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (effects->isScreenLocked()) {
        return;
    }
    qCDebug(KWIN_MOUSEMARK) << "MouseChanged pos=" << pos;

    if (modifiers == m_freedraw_modifiers && modifiers != Qt::NoModifier) {
        setState(State::FREEHAND);
    } else if (modifiers == m_arrowdraw_modifiers && modifiers != Qt::NoModifier) {
        setState(State::ARROW);
    } else {
        setState(State::NONE);
    }
    processPoint(0, pos);
}

void MouseMarkEffect::clear()
{
    drawings.clear();
    marks.clear();
    effects->addRepaintFull();
}

void MouseMarkEffect::clearLast()
{
    if (drawings.size() >= 1) { // clear anything currently being drawn first
        drawings.clear();
        effects->addRepaintFull();
    } else if (!marks.isEmpty()) {
        marks.pop_back();
        effects->addRepaintFull();
    }
}

MouseMarkEffect::Mark MouseMarkEffect::createArrow(QPointF arrow_head, QPointF arrow_tail)
{
    Mark ret;
    double angle = atan2((double)(arrow_tail.y() - arrow_head.y()), (double)(arrow_tail.x() - arrow_head.x()));
    // Arrow is made of connected lines. Make first one tail, so updates preserve the tail.
    // Last one is head, so freedraw can continue from it
    ret += arrow_tail;
    ret += arrow_head;
    ret += arrow_head + QPoint(50 * cos(angle + M_PI / 6),
                               50 * sin(angle + M_PI / 6)); // right one
    ret += arrow_head;
    ret += arrow_head + QPoint(50 * cos(angle - M_PI / 6),
                               50 * sin(angle - M_PI / 6)); // left one
    ret += arrow_head;
    return ret;
}

void MouseMarkEffect::screenLockingChanged(bool locked)
{
    if (!marks.isEmpty() || !drawings.isEmpty()) {
        effects->addRepaintFull();
    }
}

bool MouseMarkEffect::isActive() const
{
    if (effects->isScreenLocked()) {
        return false;
    }
    if (!marks.isEmpty()) {
        return true;
    }

    // only active when stuff is actually drawn
    for (const Mark &mark : drawings) {
        if (mark.size() >= 2) {
            return true;
        }
    }
    return false;
}

int MouseMarkEffect::requestedEffectChainPosition() const
{
    return 10;
}

} // namespace

#include "moc_mousemark.cpp"
