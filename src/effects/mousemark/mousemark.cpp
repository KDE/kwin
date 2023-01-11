/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mousemark.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <kwinconfig.h>
#include <kwinglplatform.h>

#include <QPainter>

#include <cmath>

namespace KWin
{

static consteval QPoint nullPoint()
{
    return QPoint(-1, -1);
}

MouseMarkEffect::MouseMarkEffect()
{
    initConfig<MouseMarkConfig>();
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear All Mouse Marks"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clear);
    a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clearLast);

    connect(effects, &EffectsHandler::mouseChanged, this, &MouseMarkEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &MouseMarkEffect::screenLockingChanged);
    reconfigure(ReconfigureAll);
    arrow_start = nullPoint();
    effects->startMousePolling(); // We require it to detect activation as well
}

MouseMarkEffect::~MouseMarkEffect()
{
    effects->stopMousePolling();
}

static int width_2 = 1;
void MouseMarkEffect::reconfigure(ReconfigureFlags)
{
    MouseMarkConfig::self()->read();
    width = MouseMarkConfig::lineWidth();
    width_2 = width / 2;
    color = MouseMarkConfig::color();
    color.setAlphaF(1.0);
}

void MouseMarkEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data); // paint normal screen
    if (marks.isEmpty() && drawing.isEmpty()) {
        return;
    }
    if (effects->isOpenGLCompositing()) {
        if (!GLPlatform::instance()->isGLES()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        }
        glLineWidth(width);
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setUseColor(true);
        vbo->setColor(color);
        const auto scale = effects->renderTargetScale();
        ShaderBinder binder(ShaderTrait::UniformColor);
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
        QVector<float> verts;
        for (const Mark &mark : std::as_const(marks)) {
            verts.clear();
            verts.reserve(mark.size() * 2);
            for (const QPoint &p : std::as_const(mark)) {
                verts << p.x() * scale << p.y() * scale;
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
            vbo->render(GL_LINE_STRIP);
        }
        if (!drawing.isEmpty()) {
            verts.clear();
            verts.reserve(drawing.size() * 2);
            for (const QPoint &p : std::as_const(drawing)) {
                verts << p.x() * scale << p.y() * scale;
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
            vbo->render(GL_LINE_STRIP);
        }
        glLineWidth(1.0);
        if (!GLPlatform::instance()->isGLES()) {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
        }
    } else if (effects->compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        painter->save();
        QPen pen(color);
        pen.setWidth(width);
        painter->setPen(pen);
        for (const Mark &mark : std::as_const(marks)) {
            drawMark(painter, mark);
        }
        drawMark(painter, drawing);
        painter->restore();
    }
}

void MouseMarkEffect::drawMark(QPainter *painter, const Mark &mark)
{
    if (mark.count() <= 1) {
        return;
    }
    for (int i = 0; i < mark.count() - 1; ++i) {
        painter->drawLine(mark[i], mark[i + 1]);
    }
}

void MouseMarkEffect::slotMouseChanged(const QPoint &pos, const QPoint &,
                                       Qt::MouseButtons, Qt::MouseButtons,
                                       Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (modifiers == (Qt::META | Qt::SHIFT | Qt::CTRL)) { // start/finish arrow
        if (arrow_start != nullPoint()) {
            marks.append(createArrow(arrow_start, pos));
            arrow_start = nullPoint();
            effects->addRepaintFull();
            return;
        } else {
            arrow_start = pos;
        }
    }
    if (arrow_start != nullPoint()) {
        return;
    }
    // TODO the shortcuts now trigger this right before they're activated
    if (modifiers == (Qt::META | Qt::SHIFT)) { // activated
        if (drawing.isEmpty()) {
            drawing.append(pos);
        }
        if (drawing.last() == pos) {
            return;
        }
        QPoint pos2 = drawing.last();
        drawing.append(pos);
        QRect repaint = QRect(std::min(pos.x(), pos2.x()), std::min(pos.y(), pos2.y()),
                              std::max(pos.x(), pos2.x()), std::max(pos.y(), pos2.y()));
        repaint.adjust(-width, -width, width, width);
        effects->addRepaint(repaint);
    } else if (!drawing.isEmpty()) {
        marks.append(drawing);
        drawing.clear();
    }
}

void MouseMarkEffect::clear()
{
    drawing.clear();
    marks.clear();
    effects->addRepaintFull();
}

void MouseMarkEffect::clearLast()
{
    if (arrow_start != nullPoint()) {
        arrow_start = nullPoint();
    } else if (!drawing.isEmpty()) {
        drawing.clear();
        effects->addRepaintFull();
    } else if (!marks.isEmpty()) {
        marks.pop_back();
        effects->addRepaintFull();
    }
}

MouseMarkEffect::Mark MouseMarkEffect::createArrow(QPoint arrow_start, QPoint arrow_end)
{
    Mark ret;
    double angle = atan2((double)(arrow_end.y() - arrow_start.y()), (double)(arrow_end.x() - arrow_start.x()));
    ret += arrow_start + QPoint(50 * cos(angle + M_PI / 6),
                                50 * sin(angle + M_PI / 6)); // right one
    ret += arrow_start;
    ret += arrow_end;
    ret += arrow_start; // it's connected lines, so go back with the middle one
    ret += arrow_start + QPoint(50 * cos(angle - M_PI / 6),
                                50 * sin(angle - M_PI / 6)); // left one
    return ret;
}

void MouseMarkEffect::screenLockingChanged(bool locked)
{
    if (!marks.isEmpty() || !drawing.isEmpty()) {
        effects->addRepaintFull();
    }
    // disable mouse polling while screen is locked.
    if (locked) {
        effects->stopMousePolling();
    } else {
        effects->startMousePolling();
    }
}

bool MouseMarkEffect::isActive() const
{
    return (!marks.isEmpty() || !drawing.isEmpty()) && !effects->isScreenLocked();
}

int MouseMarkEffect::requestedEffectChainPosition() const
{
    return 10;
}

} // namespace
