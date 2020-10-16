/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "trackmouse.h"

// KConfigSkeleton
#include "trackmouseconfig.h"

#include <QAction>
#include <QPainter>
#include <QTime>
#include <QMatrix4x4>

#include <kwinconfig.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <cmath>

namespace KWin
{

TrackMouseEffect::TrackMouseEffect()
    : m_angle(0)
{
    initConfig<TrackMouseConfig>();
    m_texture[0] = m_texture[1] = nullptr;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    m_picture[0] = m_picture[1] = nullptr;
    if ( effects->compositingType() == XRenderCompositing)
        m_angleBase = 1.57079632679489661923; // Pi/2
#endif
    if ( effects->isOpenGLCompositing() || effects->compositingType() == QPainterCompositing)
        m_angleBase = 90.0;
    m_mousePolling = false;

    m_action = new QAction(this);
    m_action->setObjectName(QStringLiteral("TrackMouse"));
    m_action->setText(i18n("Track mouse"));
    KGlobalAccel::self()->setDefaultShortcut(m_action, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_action, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), m_action);

    connect(m_action, &QAction::triggered, this, &TrackMouseEffect::toggle);

    connect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (m_mousePolling)
        effects->stopMousePolling();
    for (int i = 0; i < 2; ++i) {
        delete m_texture[i]; m_texture[i] = nullptr;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        delete m_picture[i]; m_picture[i] = nullptr;
#endif
    }
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    m_modifiers = Qt::KeyboardModifiers();
    TrackMouseConfig::self()->read();
    if (TrackMouseConfig::shift())
        m_modifiers |= Qt::ShiftModifier;
    if (TrackMouseConfig::alt())
        m_modifiers |= Qt::AltModifier;
    if (TrackMouseConfig::control())
        m_modifiers |= Qt::ControlModifier;
    if (TrackMouseConfig::meta())
        m_modifiers |= Qt::MetaModifier;

    if (m_modifiers) {
        if (!m_mousePolling)
            effects->startMousePolling();
        m_mousePolling = true;
    } else if (m_mousePolling) {
            effects->stopMousePolling();
        m_mousePolling = false;
    }
}

void TrackMouseEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    QTime t = QTime::currentTime();
    m_angle = ((t.second() % 4) * m_angleBase) + (t.msec() / 1000.0 * m_angleBase);
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    data.paint |= m_lastRect[0].adjusted(-1,-1,1,1);

    effects->prePaintScreen(data, time);
}

void TrackMouseEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen

    if ( effects->isOpenGLCompositing() && m_texture[0] && m_texture[1]) {
        ShaderBinder binder(ShaderTrait::MapTexture);
        GLShader *shader(binder.shader());
        if (!shader) {
            return;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        QMatrix4x4 matrix(data.projectionMatrix());
        const QPointF p = m_lastRect[0].topLeft() + QPoint(m_lastRect[0].width()/2.0, m_lastRect[0].height()/2.0);
        const float x = p.x()*data.xScale() + data.xTranslation();
        const float y = p.y()*data.yScale() + data.yTranslation();
        for (int i = 0; i < 2; ++i) {
            matrix.translate(x, y, 0.0);
            matrix.rotate(i ? -2*m_angle : m_angle, 0, 0, 1.0);
            matrix.translate(-x, -y, 0.0);
            QMatrix4x4 mvp(matrix);
            mvp.translate(m_lastRect[i].x(), m_lastRect[i].y());
            shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
            m_texture[i]->bind();
            m_texture[i]->render(region, m_lastRect[i]);
            m_texture[i]->unbind();
        }
        glDisable(GL_BLEND);
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if ( effects->compositingType() == XRenderCompositing && m_picture[0] && m_picture[1]) {
        float sine = sin(m_angle);
        const float cosine = cos(m_angle);
        for (int i = 0; i < 2; ++i) {
            if (i) sine = -sine;
            const float dx = m_size[i].width()/2.0;
            const float dy = m_size[i].height()/2.0;
            const xcb_render_picture_t picture = *m_picture[i];
#define DOUBLE_TO_FIXED(d) ((xcb_render_fixed_t) ((d) * 65536))
            xcb_render_transform_t xform = {
                DOUBLE_TO_FIXED( cosine ), DOUBLE_TO_FIXED( -sine ), DOUBLE_TO_FIXED( dx - cosine*dx + sine*dy ),
                DOUBLE_TO_FIXED( sine ), DOUBLE_TO_FIXED( cosine ), DOUBLE_TO_FIXED( dy - sine*dx - cosine*dy ),
                DOUBLE_TO_FIXED( 0.0 ), DOUBLE_TO_FIXED( 0.0 ), DOUBLE_TO_FIXED( 1.0 )
            };
#undef DOUBLE_TO_FIXED
            xcb_render_set_picture_transform(xcbConnection(), picture, xform);
            xcb_render_set_picture_filter(xcbConnection(), picture, 8, "bilinear", 0, nullptr);
            const QRect &rect = m_lastRect[i];
            xcb_render_composite(xcbConnection(), XCB_RENDER_PICT_OP_OVER, picture, XCB_RENDER_PICTURE_NONE,
                                 effects->xrenderBufferPicture(), 0, 0, 0, 0,
                                 qRound((rect.x()+rect.width()/2.0)*data.xScale() - rect.width()/2.0 + data.xTranslation()),
                                 qRound((rect.y()+rect.height()/2.0)*data.yScale() - rect.height()/2.0 + data.yTranslation()),
                                 rect.width(), rect.height());
        }
    }
#endif
    if (effects->compositingType() == QPainterCompositing && !m_image[0].isNull() && !m_image[1].isNull()) {
        QPainter *painter = effects->scenePainter();
        const QPointF p = m_lastRect[0].topLeft() + QPoint(m_lastRect[0].width()/2.0, m_lastRect[0].height()/2.0);
        for (int i = 0; i < 2; ++i) {
            painter->save();
            painter->translate(p.x(), p.y());
            painter->rotate(i ? -2*m_angle : m_angle);
            painter->translate(-p.x(), -p.y());
            painter->drawImage(m_lastRect[i], m_image[i]);
            painter->restore();
        }
    }
}

void TrackMouseEffect::postPaintScreen()
{
    effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    effects->postPaintScreen();
}

bool TrackMouseEffect::init()
{
    effects->makeOpenGLContextCurrent();
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (!(m_texture[0] || m_picture[0] || !m_image[0].isNull())) {
        loadTexture();
        if (!(m_texture[0] || m_picture[0] || !m_image[0].isNull()))
            return false;
    }
#else
    if (!m_texture[0] || m_image[0].isNull()) {
        loadTexture();
        if (!m_texture[0] || m_image[0].isNull())
            return false;
    }
#endif
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    m_angle = 0;
    return true;
}

void TrackMouseEffect::toggle()
{
    switch (m_state) {
    case State::ActivatedByModifiers:
        m_state = State::ActivatedByShortcut;
        break;

    case State::ActivatedByShortcut:
        m_state = State::Inactive;
        break;

    case State::Inactive:
        if (!init()) {
            return;
        }
        m_state = State::ActivatedByShortcut;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    effects->addRepaint(m_lastRect[0].adjusted(-1, -1, 1, 1));
}

void TrackMouseEffect::slotMouseChanged(const QPoint&, const QPoint&,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (!m_mousePolling) { // we didn't ask for it but maybe someone else did...
        return;
    }

    switch (m_state) {
    case State::ActivatedByModifiers:
        if (modifiers == m_modifiers) {
            return;
        }
        m_state = State::Inactive;
        break;

    case State::ActivatedByShortcut:
        return;

    case State::Inactive:
        if (modifiers != m_modifiers) {
            return;
        }
        if (!init()) {
            return;
        }
        m_state = State::ActivatedByModifiers;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    effects->addRepaint(m_lastRect[0].adjusted(-1, -1, 1, 1));
}

void TrackMouseEffect::loadTexture()
{
    QString f[2] = {QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("tm_outer.png")),
                    QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("tm_inner.png"))};
    if (f[0].isEmpty() || f[1].isEmpty())
        return;

    for (int i = 0; i < 2; ++i) {
        if ( effects->isOpenGLCompositing()) {
            QImage img(f[i]);
            m_texture[i] = new GLTexture(img);
            m_lastRect[i].setSize(img.size());
        }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if ( effects->compositingType() == XRenderCompositing) {
            QImage pixmap(f[i]);
            m_picture[i] = new XRenderPicture(pixmap);
            m_size[i] = pixmap.size();
            m_lastRect[i].setSize(pixmap.size());
        }
#endif
        if (effects->compositingType() == QPainterCompositing) {
            m_image[i] = QImage(f[i]);
            m_lastRect[i].setSize(m_image[i].size());
        }
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_state != State::Inactive;
}

} // namespace
