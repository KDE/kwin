/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "magnifier.h"
// KConfigSkeleton
#include "magnifierconfig.h"

#include <QAction>
#include <kwinconfig.h>
#include <kstandardaction.h>

#include <kwinglutils.h>
#include <KGlobalAccel>

namespace KWin
{

const int FRAME_WIDTH = 5;

MagnifierEffect::MagnifierEffect()
    : zoom(1)
    , target_zoom(1)
    , polling(false)
    , m_lastPresentTime(std::chrono::milliseconds::zero())
    , m_texture(nullptr)
    , m_fbo(nullptr)
{
    initConfig<MagnifierConfig>();
    QAction* a;
    a = KStandardAction::zoomIn(this, &MagnifierEffect::zoomIn, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Equal, a);

    a = KStandardAction::zoomOut(this, &MagnifierEffect::zoomOut, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Minus, a);

    a = KStandardAction::actualSize(this, &MagnifierEffect::toggle, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_0, a);

    connect(effects, &EffectsHandler::mouseChanged, this, &MagnifierEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowDamaged, this, &MagnifierEffect::slotWindowDamaged);

    reconfigure(ReconfigureAll);
}

MagnifierEffect::~MagnifierEffect()
{
    delete m_fbo;
    delete m_texture;
    // Save the zoom value.
    MagnifierConfig::setInitialZoom(target_zoom);
    MagnifierConfig::self()->save();
}

bool MagnifierEffect::supported()
{
    return effects->isOpenGLCompositing() && GLRenderTarget::blitSupported();
}

void MagnifierEffect::reconfigure(ReconfigureFlags)
{
    MagnifierConfig::self()->read();
    int width, height;
    width = MagnifierConfig::width();
    height = MagnifierConfig::height();
    magnifier_size = QSize(width, height);
    // Load the saved zoom value.
    target_zoom = MagnifierConfig::initialZoom();
    if (target_zoom != zoom)
        toggle();
}

void MagnifierEffect::prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
    const int time = m_lastPresentTime.count() ? (presentTime - m_lastPresentTime).count() : 0;

    if (zoom != target_zoom) {
        double diff = time / animationTime(500.0);
        if (target_zoom > zoom)
            zoom = qMin(zoom * qMax(1 + diff, 1.2), target_zoom);
        else {
            zoom = qMax(zoom * qMin(1 - diff, 0.8), target_zoom);
            if (zoom == 1.0) {
                // zoom ended - delete FBO and texture
                delete m_fbo;
                delete m_texture;
                m_fbo = nullptr;
                m_texture = nullptr;
            }
        }
    }

    if (zoom != target_zoom) {
        m_lastPresentTime = presentTime;
    } else {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    }

    effects->prePaintScreen(data, presentTime);
    if (zoom != 1.0)
        data.paint |= magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH);
}

void MagnifierEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (zoom != 1.0) {
        // get the right area from the current rendered screen
        const QRect area = magnifierArea();
        const QPoint cursor = cursorPos();

        QRect srcArea(cursor.x() - (double)area.width() / (zoom*2),
                      cursor.y() - (double)area.height() / (zoom*2),
                      (double)area.width() / zoom, (double)area.height() / zoom);
        if (effects->isOpenGLCompositing()) {
            m_fbo->blitFromFramebuffer(srcArea);
            // paint magnifier
            m_texture->bind();
            auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
            QMatrix4x4 mvp;
            const QSize size = effects->virtualScreenSize();
            mvp.ortho(0, size.width(), size.height(), 0, 0, 65535);
            mvp.translate(area.x(), area.y());
            s->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
            m_texture->render(infiniteRegion(), area);
            ShaderManager::instance()->popShader();
            m_texture->unbind();
            QVector<float> verts;
            GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();
            vbo->setColor(QColor(0, 0, 0));
            const QRectF areaF = area;
            // top frame
            verts << areaF.right() + FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.top();
            verts << areaF.left() - FRAME_WIDTH << areaF.top();
            verts << areaF.right() + FRAME_WIDTH << areaF.top();
            verts << areaF.right() + FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            // left frame
            verts << areaF.left() << areaF.top() - FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.left() << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.left() << areaF.top() - FRAME_WIDTH;
            // right frame
            verts << areaF.right() + FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            verts << areaF.right() << areaF.top() - FRAME_WIDTH;
            verts << areaF.right() << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.right() << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.right() + FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.right() + FRAME_WIDTH << areaF.top() - FRAME_WIDTH;
            // bottom frame
            verts << areaF.right() + FRAME_WIDTH << areaF.bottom();
            verts << areaF.left() - FRAME_WIDTH << areaF.bottom();
            verts << areaF.left() - FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.left() - FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.right() + FRAME_WIDTH << areaF.bottom() + FRAME_WIDTH;
            verts << areaF.right() + FRAME_WIDTH << areaF.bottom();
            vbo->setData(verts.size() / 2, 2, verts.constData(), nullptr);

            ShaderBinder binder(ShaderTrait::UniformColor);
            binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
            vbo->render(GL_TRIANGLES);
        }
    }
}

void MagnifierEffect::postPaintScreen()
{
    if (zoom != target_zoom) {
        QRect framedarea = magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH);
        effects->addRepaint(framedarea);
    }
    effects->postPaintScreen();
}

QRect MagnifierEffect::magnifierArea(QPoint pos) const
{
    return QRect(pos.x() - magnifier_size.width() / 2, pos.y() - magnifier_size.height() / 2,
                 magnifier_size.width(), magnifier_size.height());
}

void MagnifierEffect::zoomIn()
{
    target_zoom *= 1.2;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    if (effects->isOpenGLCompositing() && !m_texture) {
        effects->makeOpenGLContextCurrent();
        m_texture = new GLTexture(GL_RGBA8, magnifier_size.width(), magnifier_size.height());
        m_texture->setYInverted(false);
        m_fbo = new GLRenderTarget(*m_texture);
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::zoomOut()
{
    target_zoom /= 1.2;
    if (target_zoom <= 1) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
        if (zoom == target_zoom) {
            effects->makeOpenGLContextCurrent();
            delete m_fbo;
            delete m_texture;
            m_fbo = nullptr;
            m_texture = nullptr;
        }
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::toggle()
{
    if (zoom == 1.0) {
        if (target_zoom == 1.0) {
            target_zoom = 2;
        }
        if (!polling) {
            polling = true;
            effects->startMousePolling();
        }
        if (effects->isOpenGLCompositing() && !m_texture) {
            effects->makeOpenGLContextCurrent();
            m_texture = new GLTexture(GL_RGBA8, magnifier_size.width(), magnifier_size.height());
            m_texture->setYInverted(false);
            m_fbo = new GLRenderTarget(*m_texture);
        }
    } else {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::slotMouseChanged(const QPoint& pos, const QPoint& old,
                                   Qt::MouseButtons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (pos != old && zoom != 1)
        // need full repaint as we might lose some change events on fast mouse movements
        // see Bug 187658
        effects->addRepaintFull();
}

void MagnifierEffect::slotWindowDamaged()
{
    if (isActive()) {
        effects->addRepaint(magnifierArea());
    }
}

bool MagnifierEffect::isActive() const
{
    return zoom != 1.0 || zoom != target_zoom;
}

} // namespace

