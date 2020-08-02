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
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <kwinxrenderutils.h>
#include <xcb/render.h>
#endif
#include <KGlobalAccel>

namespace KWin
{

const int FRAME_WIDTH = 5;

MagnifierEffect::MagnifierEffect()
    : zoom(1)
    , target_zoom(1)
    , polling(false)
    , m_texture(nullptr)
    , m_fbo(nullptr)
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    , m_pixmap(XCB_PIXMAP_NONE)
#endif
{
    initConfig<MagnifierConfig>();
    QAction* a;
    a = KStandardAction::zoomIn(this, SLOT(zoomIn()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Equal, a);

    a = KStandardAction::zoomOut(this, SLOT(zoomOut()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Minus, a);

    a = KStandardAction::actualSize(this, SLOT(toggle()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_0, a);

    connect(effects, &EffectsHandler::mouseChanged, this, &MagnifierEffect::slotMouseChanged);

    reconfigure(ReconfigureAll);
}

MagnifierEffect::~MagnifierEffect()
{
    delete m_fbo;
    delete m_texture;
    destroyPixmap();
    // Save the zoom value.
    MagnifierConfig::setInitialZoom(target_zoom);
    MagnifierConfig::self()->save();
}

void MagnifierEffect::destroyPixmap()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() != XRenderCompositing) {
        return;
    }
    m_picture.reset();
    if (m_pixmap != XCB_PIXMAP_NONE) {
        xcb_free_pixmap(xcbConnection(), m_pixmap);
        m_pixmap = XCB_PIXMAP_NONE;
    }
#endif
}

bool MagnifierEffect::supported()
{
    return  effects->compositingType() == XRenderCompositing ||
            (effects->isOpenGLCompositing() && GLRenderTarget::blitSupported());
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

void MagnifierEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
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
                destroyPixmap();
            }
        }
    }
    effects->prePaintScreen(data, time);
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
        if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (m_pixmap == XCB_PIXMAP_NONE || m_pixmapSize != srcArea.size()) {
                destroyPixmap();
                m_pixmap = xcb_generate_id(xcbConnection());
                m_pixmapSize = srcArea.size();
                xcb_create_pixmap(xcbConnection(), 32, m_pixmap, x11RootWindow(), m_pixmapSize.width(), m_pixmapSize.height());
                m_picture.reset(new XRenderPicture(m_pixmap, 32));
            }
#define DOUBLE_TO_FIXED(d) ((xcb_render_fixed_t) ((d) * 65536))
            static const xcb_render_transform_t identity = {
                DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1)
            };
            static xcb_render_transform_t xform = {
                DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1)
            };
            xcb_render_composite(xcbConnection(), XCB_RENDER_PICT_OP_SRC, effects->xrenderBufferPicture(), 0, *m_picture,
                                srcArea.x(), srcArea.y(), 0, 0, 0, 0, srcArea.width(), srcArea.height());
            xcb_flush(xcbConnection());
            xform.matrix11 = DOUBLE_TO_FIXED(1.0/zoom);
            xform.matrix22 = DOUBLE_TO_FIXED(1.0/zoom);
#undef DOUBLE_TO_FIXED
            xcb_render_set_picture_transform(xcbConnection(), *m_picture, xform);
            xcb_render_set_picture_filter(xcbConnection(), *m_picture, 4, const_cast<char*>("good"), 0, nullptr);
            xcb_render_composite(xcbConnection(), XCB_RENDER_PICT_OP_SRC, *m_picture, 0, effects->xrenderBufferPicture(),
                                 0, 0, 0, 0, area.x(), area.y(), area.width(), area.height() );
            xcb_render_set_picture_filter(xcbConnection(), *m_picture, 4, const_cast<char*>("fast"), 0, nullptr);
            xcb_render_set_picture_transform(xcbConnection(), *m_picture, identity);
            const xcb_rectangle_t rects[4] = {
                { int16_t(area.x()+FRAME_WIDTH), int16_t(area.y()), uint16_t(area.width()-FRAME_WIDTH), uint16_t(FRAME_WIDTH)},
                { int16_t(area.right()-FRAME_WIDTH), int16_t(area.y()+FRAME_WIDTH), uint16_t(FRAME_WIDTH), uint16_t(area.height()-FRAME_WIDTH)},
                { int16_t(area.x()), int16_t(area.bottom()-FRAME_WIDTH), uint16_t(area.width()-FRAME_WIDTH), uint16_t(FRAME_WIDTH)},
                { int16_t(area.x()), int16_t(area.y()), uint16_t(FRAME_WIDTH), uint16_t(area.height()-FRAME_WIDTH)}
            };
            xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, effects->xrenderBufferPicture(),
                                       preMultiply(QColor(0,0,0,255)), 4, rects);
#endif
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
            destroyPixmap();
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

bool MagnifierEffect::isActive() const
{
    return zoom != 1.0 || zoom != target_zoom;
}

} // namespace

