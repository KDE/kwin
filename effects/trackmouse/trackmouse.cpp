/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "trackmouse.h"

#include <QTime>
#include <QMatrix4x4>

#include <kwinconfig.h>
#include <kwinglutils.h>

#include <kglobal.h>
#include <kstandarddirs.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <KDE/KConfigGroup>
#include <KDE/KLocale>

#include <math.h>
#include <X11/extensions/Xrender.h>

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT(trackmouse, TrackMouseEffect)

TrackMouseEffect::TrackMouseEffect()
    : m_active(false)
    , m_angle(0)
{
    m_texture[0] = m_texture[1] = 0;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    m_pixmap[0] = m_pixmap[1] = 0;
    if ( effects->compositingType() == XRenderCompositing)
        m_angleBase = 1.57079632679489661923; // Pi/2
#endif
    if ( effects->compositingType() == OpenGLCompositing)
        m_angleBase = 90.0;
    m_mousePolling = false;
    KActionCollection *actionCollection = new KActionCollection(this);
    m_action = static_cast< KAction* >(actionCollection->addAction("TrackMouse"));
    m_action->setText(i18n("Track mouse"));
    m_action->setGlobalShortcut(KShortcut());

    connect(m_action, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
                     SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (m_mousePolling)
        effects->stopMousePolling();
    for (int i = 0; i < 2; ++i) {
        delete m_texture[i]; m_texture[i] = 0;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        delete m_pixmap[i]; m_pixmap[i] = 0;
#endif
    }
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    m_modifiers = 0;
    KConfigGroup conf = effects->effectConfig("TrackMouse");
    if (conf.readEntry("Shift", false))
        m_modifiers |= Qt::ShiftModifier;
    if (conf.readEntry("Alt", false))
        m_modifiers |= Qt::AltModifier;
    if (conf.readEntry("Control", true))
        m_modifiers |= Qt::ControlModifier;
    if (conf.readEntry("Meta", true))
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
    if (m_active) {
        QTime t = QTime::currentTime();
        m_angle = ((t.second() % 4) * m_angleBase) + (t.msec() / 1000.0 * m_angleBase);
        m_lastRect[0].moveCenter(cursorPos());
        m_lastRect[1].moveCenter(cursorPos());
        data.paint |= m_lastRect[0].adjusted(-1,-1,1,1);
    }
    effects->prePaintScreen(data, time);
}

void TrackMouseEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (!m_active)
        return;

    if ( effects->compositingType() == OpenGLCompositing && m_texture[0] && m_texture[1]) {
        GLShader *shader(0);
        QMatrix4x4 modelview;
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->pushShader(ShaderManager::GenericShader);
            shader = ShaderManager::instance()->getBoundShader();
            modelview = shader->getUniformMatrix4x4("modelview");
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        QMatrix4x4 matrix(modelview);
        const QPointF p = m_lastRect[0].topLeft() + QPoint(m_lastRect[0].width()/2.0, m_lastRect[0].height()/2.0);
        for (int i = 0; i < 2; ++i) {
            matrix.translate(p.x(), p.y(), 0.0);
            matrix.rotate(i ? -2*m_angle : m_angle, 0, 0, 1.0);
            matrix.translate(-p.x(), -p.y(), 0.0);
            if (shader)
                shader->setUniform(GLShader::ModelViewMatrix, matrix);
            else
                pushMatrix(matrix);
            m_texture[i]->bind();
            m_texture[i]->render(region, m_lastRect[i]);
            m_texture[i]->unbind();
            if (!shader)
                popMatrix();
        }
        glDisable(GL_BLEND);
        if (ShaderManager::instance()->isValid()) {
            shader->setUniform(GLShader::ModelViewMatrix, modelview);
            ShaderManager::instance()->popShader();
        }
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if ( effects->compositingType() == XRenderCompositing && m_pixmap[0] && m_pixmap[1]) {
        float sine = sin(m_angle);
        const float cosine = cos(m_angle);
        for (int i = 0; i < 2; ++i) {
            if (i) sine = -sine;
            const float dx = m_pixmap[i]->width()/2.0;
            const float dy = m_pixmap[i]->height()/2.0;
            XTransform xform = {{
                { XDoubleToFixed( cosine ), XDoubleToFixed( -sine ), XDoubleToFixed( dx - cosine*dx + sine*dy ) },
                { XDoubleToFixed( sine ), XDoubleToFixed( cosine ), XDoubleToFixed( dy - sine*dx - cosine*dy ) },
                { XDoubleToFixed( 0.0 ), XDoubleToFixed( 0.0 ), XDoubleToFixed( 1.0 ) }
            }};
            XRenderSetPictureTransform(display(), m_pixmap[i]->x11PictureHandle(), &xform );
            XRenderSetPictureFilter( display(), m_pixmap[i]->x11PictureHandle(), FilterBilinear, 0, 0 );
            XRenderComposite(display(), PictOpOver, m_pixmap[i]->x11PictureHandle(), 0,
                             effects->xrenderBufferPicture(), 0, 0, 0, 0,
                             m_lastRect[i].x(), m_lastRect[i].y(), m_lastRect[i].width(), m_lastRect[i].height());
        }
    }
#endif
}

void TrackMouseEffect::postPaintScreen()
{
    if (m_active) {
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    }
    effects->postPaintScreen();
}

bool TrackMouseEffect::init()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (!(m_texture[0] || m_pixmap[0])) {
        loadTexture();
        if (!(m_texture[0] || m_pixmap[0]))
            return false;
    }
#else
    if (!m_texture[0]) {
        loadTexture();
        if (!m_texture[0])
            return false;
    }
#endif
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    m_active = true;
    m_angle = 0;
    return true;
}

void TrackMouseEffect::toggle()
{
    if (m_mousePolling)
        return;

    if (m_active) {
        m_active = false;
    } else if (!init()) {
        return;
    }
    effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
}

void TrackMouseEffect::slotMouseChanged(const QPoint&, const QPoint&,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (!m_mousePolling) // we didn't ask for it but maybe someone else did...
        return;
    if (m_modifiers && modifiers == m_modifiers) {
        if (!m_active && !init()) {
            return;
        }
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    } else if (m_active) {
        m_active = false;
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    }
}

void TrackMouseEffect::loadTexture()
{
    QString f[2] = {KGlobal::dirs()->findResource("appdata", "tm_outer.png"),
                    KGlobal::dirs()->findResource("appdata", "tm_inner.png")};
    if (f[0].isEmpty() || f[1].isEmpty())
        return;

    for (int i = 0; i < 2; ++i) {
        if ( effects->compositingType() == OpenGLCompositing) {
            QImage img(f[i]);
            m_texture[i] = new GLTexture(img);
            m_lastRect[i].setSize(img.size());
        }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if ( effects->compositingType() == XRenderCompositing) {
            m_pixmap[i] = new QPixmap(f[i]);
            m_lastRect[i].setSize(m_pixmap[i]->size());
        }
#endif
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_active;
}

} // namespace
