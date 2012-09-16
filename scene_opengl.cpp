/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.

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


/*
 This is the OpenGL-based compositing code. It is the primary and most powerful
 compositing backend.

Sources and other compositing managers:
=======================================

- http://opengl.org
    - documentation
        - OpenGL Redbook (http://opengl.org/documentation/red_book/ - note it's only version 1.1)
        - GLX docs (http://opengl.org/documentation/specs/glx/glx1.4.pdf)
        - extensions docs (http://www.opengl.org/registry/)

- glcompmgr
    - http://lists.freedesktop.org/archives/xorg/2006-July/017006.html ,
    - http://www.mail-archive.com/compiz%40lists.freedesktop.org/msg00023.html
    - simple and easy to understand
    - works even without texture_from_pixmap extension
    - claims to support several different gfx cards
    - compile with something like
      "gcc -Wall glcompmgr-0.5.c `pkg-config --cflags --libs glib-2.0` -lGL -lXcomposite -lXdamage -L/usr/X11R6/lib"

- compiz
    - git clone git://anongit.freedesktop.org/git/xorg/app/compiz
    - the ultimate <whatever>
    - glxcompmgr
        - git clone git://anongit.freedesktop.org/git/xorg/app/glxcompgr
        - a rather old version of compiz, but also simpler and as such simpler
            to understand

- beryl
    - a fork of Compiz
    - http://beryl-project.org
    - git clone git://anongit.beryl-project.org/beryl/beryl-core (or beryl-plugins etc. ,
        the full list should be at git://anongit.beryl-project.org/beryl/)

- libcm (metacity)
    - cvs -d :pserver:anonymous@anoncvs.gnome.org:/cvs/gnome co libcm
    - not much idea about it, the model differs a lot from KWin/Compiz/Beryl
    - does not seem to be very powerful or with that much development going on

*/

#include "scene_opengl.h"

#include <kxerrorhandler.h>

#include <kwinglcolorcorrection.h>
#include <kwinglplatform.h>

#include "utils.h"
#include "client.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "overlaywindow.h"

#include <math.h>

// turns on checks for opengl errors in various places (for easier finding of them)
// normally only few of them are enabled
//#define CHECK_GL_ERROR

#include <X11/extensions/Xcomposite.h>

#include <qpainter.h>
#include <QDesktopWidget>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

namespace KWin
{

extern int currentRefreshRate();

//****************************************
// SceneOpenGL
//****************************************

bool SceneOpenGL::db; // destination drawable is double-buffered

#ifdef KWIN_HAVE_OPENGLES
#include "scene_opengl_egl.cpp"
#else
#include "scene_opengl_glx.cpp"
#endif

void SceneOpenGL::idle()
{
    flushBuffer(m_lastMask, m_lastDamage);
    Scene::idle();
}

bool SceneOpenGL::initFailed() const
{
    return !init_ok;
}

ColorCorrection* SceneOpenGL::colorCorrection()
{
    return m_colorCorrection;
}

void SceneOpenGL::initColorCorrection()
{
    kDebug(1212) << "Color correction:" << options->isColorCorrected();
    m_colorCorrection->setEnabled(options->isColorCorrected());
    connect(m_colorCorrection, SIGNAL(changed()), Compositor::self(), SLOT(addRepaintFull()));
    connect(options, SIGNAL(colorCorrectedChanged()), Compositor::self(), SLOT(slotReinitialize()));
}

bool SceneOpenGL::selectMode()
{
    if (!initDrawableConfigs())
        return false;
    return true;
}

QMatrix4x4 SceneOpenGL::transformation(int mask, const ScreenPaintData &data) const
{
    QMatrix4x4 matrix;

    if (!(mask & PAINT_SCREEN_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // cannot use data.rotation->applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}

void SceneOpenGL::paintGenericScreen(int mask, ScreenPaintData data)
{
    ShaderManager *shaderManager = ShaderManager::instance();
    const bool useShader = shaderManager->isValid();
    const QMatrix4x4 matrix = transformation(mask, data);

    if (useShader) {
        GLShader *shader = shaderManager->pushShader(ShaderManager::GenericShader);
        shader->setUniform(GLShader::ScreenTransformation, matrix);
    } else {
        pushMatrix(matrix);
    }

    Scene::paintGenericScreen(mask, data);

    if (useShader)
        shaderManager->popShader();
    else
        popMatrix();
}

void SceneOpenGL::paintBackground(QRegion region)
{
    PaintClipper pc(region);
    if (!PaintClipper::clip()) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    if (pc.clip() && pc.paintArea().isEmpty())
        return; // no background to paint
    QVector<float> verts;
    for (PaintClipper::Iterator iterator; !iterator.isDone(); iterator.next()) {
        QRect r = iterator.boundingRect();
        verts << r.x() + r.width() << r.y();
        verts << r.x() << r.y();
        verts << r.x() << r.y() + r.height();
        verts << r.x() << r.y() + r.height();
        verts << r.x() + r.width() << r.y() + r.height();
        verts << r.x() + r.width() << r.y();
    }
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setData(verts.count() / 2, 2, verts.data(), NULL);
    const bool useShader = ShaderManager::instance()->isValid();
    if (useShader) {
        GLShader *shader = ShaderManager::instance()->pushShader(ShaderManager::ColorShader);
        shader->setUniform(GLShader::Offset, QVector2D(0, 0));
    }
    vbo->render(GL_TRIANGLES);
    if (useShader) {
        ShaderManager::instance()->popShader();
    }
}

void SceneOpenGL::finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (options->isColorCorrected()) {
        // Split the painting for separate screens
        int numScreens = Workspace::self()->numScreens();
        for (int screen = 0; screen < numScreens; ++ screen) {
            QRegion regionForScreen(region);
            if (numScreens > 1)
                regionForScreen = region.intersected(Workspace::self()->screenGeometry(screen));

            data.setScreen(screen);
            performPaintWindow(w, mask, regionForScreen, data);
        }
    } else {
        performPaintWindow(w, mask, region, data);
    }
}

void SceneOpenGL::performPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (mask & PAINT_WINDOW_LANCZOS) {
        if (m_lanczosFilter.isNull()) {
            m_lanczosFilter = new LanczosFilter(this);
            // recreate the lanczos filter when the screen gets resized
            connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), m_lanczosFilter.data(), SLOT(deleteLater()));
            connect(QApplication::desktop(), SIGNAL(resized(int)), m_lanczosFilter.data(), SLOT(deleteLater()));
        }
        m_lanczosFilter.data()->performPaint(w, mask, region, data);
    } else
        w->sceneWindow()->performPaint(mask, region, data);
}

void SceneOpenGL::windowAdded(Toplevel* c)
{
    assert(!windows.contains(c));
    Window *w = new Window(c);
    windows[ c ] = w;
    w->setScene(this);
    connect(c, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), SLOT(windowOpacityChanged(KWin::Toplevel*)));
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SLOT(windowGeometryShapeChanged(KWin::Toplevel*)));
    connect(c, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), SLOT(windowClosed(KWin::Toplevel*,KWin::Deleted*)));
    c->effectWindow()->setSceneWindow(windows[ c ]);
    c->getShadow();
    windows[ c ]->updateShadow(c->shadow());
}

void SceneOpenGL::windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted)
{
    assert(windows.contains(c));
    if (deleted != NULL) {
        // replace c with deleted
        Window* w = windows.take(c);
        w->updateToplevel(deleted);
        if (w->shadow()) {
            w->shadow()->setToplevel(deleted);
        }
        windows[ deleted ] = w;
    } else {
        delete windows.take(c);
        c->effectWindow()->setSceneWindow(NULL);
    }
}

void SceneOpenGL::windowDeleted(Deleted* c)
{
    assert(windows.contains(c));
    delete windows.take(c);
    c->effectWindow()->setSceneWindow(NULL);
}

void SceneOpenGL::windowGeometryShapeChanged(KWin::Toplevel* c)
{
    if (!windows.contains(c))    // this is ok, shape is not valid
        return;                 // by default
    Window* w = windows[ c ];
    w->discardShape();
    w->checkTextureSize();
}

void SceneOpenGL::windowOpacityChanged(KWin::Toplevel* t)
{
    Q_UNUSED(t)
#if 0 // not really needed, windows are painted on every repaint
    // and opacity is used when applying texture, not when
    // creating it
    if (!windows.contains(c))    // this is ok, texture is created
        return;                 // on demand
    Window* w = windows[ c ];
    w->discardTexture();
#endif
}

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::Texture::Texture() : GLTexture(*new TexturePrivate())
{
}

SceneOpenGL::Texture::Texture(TexturePrivate& dd) : GLTexture(dd)
{
}

SceneOpenGL::Texture::Texture(const SceneOpenGL::Texture& tex) : GLTexture(*tex.d_ptr)
{
}

SceneOpenGL::Texture::Texture(const Pixmap& pix, const QSize& size, int depth)
    : GLTexture(*new TexturePrivate())
{
    load(pix, size, depth);
}

SceneOpenGL::Texture::Texture(const QPixmap& pix, GLenum target)
    : GLTexture(*new TexturePrivate())
{
    load(pix, target);
}

SceneOpenGL::Texture::~Texture()
{
}

SceneOpenGL::Texture& SceneOpenGL::Texture::operator = (const SceneOpenGL::Texture& tex)
{
    d_ptr = tex.d_ptr;
    return *this;
}

void SceneOpenGL::Texture::discard()
{
    d_ptr = new TexturePrivate();
}

bool SceneOpenGL::Texture::load(const Pixmap& pix, const QSize& size,
                                int depth)
{
    if (pix == None)
        return false;
    return load(pix, size, depth,
                QRegion(0, 0, size.width(), size.height()));
}

bool SceneOpenGL::Texture::load(const QImage& image, GLenum target)
{
    if (image.isNull())
        return false;
    return load(QPixmap::fromImage(image), target);
}

bool SceneOpenGL::Texture::load(const QPixmap& pixmap, GLenum target)
{
    if (pixmap.isNull())
        return false;

    // Checking whether QPixmap comes with its own X11 Pixmap
    if (Extensions::nonNativePixmaps()) {
        return GLTexture::load(pixmap.toImage(), target);
    }

    // use the X11 pixmap provided by Qt
    return load(pixmap.handle(), pixmap.size(), pixmap.depth());
}

//****************************************
// SceneOpenGL::Window
//****************************************

SceneOpenGL::Window::Window(Toplevel* c)
    : Scene::Window(c)
    , texture()
    , topTexture()
    , leftTexture()
    , rightTexture()
    , bottomTexture()
    , m_scene(NULL)
{
}

SceneOpenGL::Window::~Window()
{
    discardTexture();
}

// Bind the window pixmap to an OpenGL texture.
bool SceneOpenGL::Window::bindTexture()
{
    if (!texture.isNull()) {
        if (!toplevel->damage().isEmpty()) {
            // mipmaps need to be updated
            texture.setDirty();
            toplevel->resetDamage(QRect(toplevel->clientPos(), toplevel->clientSize()));
        }
        return true;
    }
    // Get the pixmap with the window contents
    Pixmap pix = toplevel->windowPixmap();
    if (pix == None)
        return false;

    bool success = texture.load(pix, toplevel->size(), toplevel->depth(),
                                toplevel->damage());

    if (success)
        toplevel->resetDamage(QRect(toplevel->clientPos(), toplevel->clientSize()));
    else
        kDebug(1212) << "Failed to bind window";
    return success;
}

void SceneOpenGL::Window::discardTexture()
{
    texture.discard();
    topTexture.discard();
    leftTexture.discard();
    rightTexture.discard();
    bottomTexture.discard();
}

// This call is used in SceneOpenGL::windowGeometryShapeChanged(),
// which originally called discardTexture(), however this was causing performance
// problems with the launch feedback icon - large number of texture rebinds.
// Since the launch feedback icon does not resize, only changes shape, it
// is not necessary to rebind the texture (with no strict binding), therefore
// discard the texture only if size changes.
void SceneOpenGL::Window::checkTextureSize()
{
    if (texture.size() != size())
        discardTexture();
}

// when the window's composite pixmap is discarded, undo binding it to the texture
void SceneOpenGL::Window::pixmapDiscarded()
{
    texture.discard();
}

QMatrix4x4 SceneOpenGL::Window::transformation(int mask, const WindowPaintData &data) const
{
    QMatrix4x4 matrix;
    matrix.translate(x(), y());

    if (!(mask & PAINT_WINDOW_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // cannot use data.rotation.applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}

// paint the window
void SceneOpenGL::Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (region.isEmpty())
        return;

    bool hardwareClipping = region != infiniteRegion() && (mask & PAINT_WINDOW_TRANSFORMED);
    if (region != infiniteRegion() && !hardwareClipping) {
        WindowQuadList quads;
        const QRegion filterRegion = region.translated(-x(), -y());
        // split all quads in bounding rect with the actual rects in the region
        foreach (const WindowQuad &quad, data.quads) {
            foreach (const QRect &r, filterRegion.rects()) {
                const QRectF rf(r);
                const QRectF quadRect(QPointF(quad.left(), quad.top()), QPointF(quad.right(), quad.bottom()));
                // case 1: completely contains, include and do not check other rects
                if (rf.contains(quadRect)) {
                    quads << quad;
                    break;
                }
                // case 2: intersection
                if (rf.intersects(quadRect)) {
                    const QRectF intersected = rf.intersected(quadRect);
                    quads << quad.makeSubQuad(intersected.left(), intersected.top(), intersected.right(), intersected.bottom());
                }
            }
        }
        data.quads = quads;
    }

    if (!bindTexture()) {
        return;
    }

    if (hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    // Update the texture filter
    if (options->glSmoothScale() != 0 &&
        (mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        filter = ImageFilterGood;
    else
        filter = ImageFilterFast;

    texture.setFilter(filter == ImageFilterGood ? GL_LINEAR : GL_NEAREST);

    bool sceneShader = false;

    if (!data.shader && ShaderManager::instance()->isValid()) {
        // set the shader for uniform initialising in paint decoration
        if ((mask & PAINT_WINDOW_TRANSFORMED) || (mask & PAINT_SCREEN_TRANSFORMED)) {
            data.shader = ShaderManager::instance()->pushShader(ShaderManager::GenericShader);
        } else {
            data.shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
            data.shader->setUniform(GLShader::Offset, QVector2D(x(), y()));
        }
        m_scene->colorCorrection()->setupForOutput(data.screen());
        sceneShader = true;
    }

    const QMatrix4x4 windowTransformation = transformation(mask, data);

    if (data.shader)
        data.shader->setUniform(GLShader::WindowTransformation, windowTransformation);

    if (!sceneShader)
        pushMatrix(windowTransformation);

    WindowQuadList decoration = data.quads.select(WindowQuadDecoration);

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();

    // shadow
    if (m_shadow) {
        paintShadow(region, data, hardwareClipping);
    }
    // decorations
    Client *client = dynamic_cast<Client*>(toplevel);
    Deleted *deleted = dynamic_cast<Deleted*>(toplevel);
    if (client || deleted) {
        bool noBorder = true;
        bool updateDeco = false;
        const QPixmap *left = NULL;
        const QPixmap *top = NULL;
        const QPixmap *right = NULL;
        const QPixmap *bottom = NULL;
        QRect topRect, leftRect, rightRect, bottomRect;
        if (client && !client->noBorder()) {
            noBorder = false;
            updateDeco = client->decorationPixmapRequiresRepaint();
            client->ensureDecorationPixmapsPainted();

            client->layoutDecorationRects(leftRect, topRect, rightRect, bottomRect, Client::WindowRelative);

            left   = client->leftDecoPixmap();
            top    = client->topDecoPixmap();
            right  = client->rightDecoPixmap();
            bottom = client->bottomDecoPixmap();
        }
        if (deleted && !deleted->noBorder()) {
            noBorder = false;
            left   = deleted->leftDecoPixmap();
            top    = deleted->topDecoPixmap();
            right  = deleted->rightDecoPixmap();
            bottom = deleted->bottomDecoPixmap();
            deleted->layoutDecorationRects(leftRect, topRect, rightRect, bottomRect);
        }
        if (!noBorder) {
            WindowQuadList topList, leftList, rightList, bottomList;

            foreach (const WindowQuad & quad, decoration) {
                if (topRect.contains(QPoint(quad.originalLeft(), quad.originalTop()))) {
                    topList.append(quad);
                    continue;
                }
                if (bottomRect.contains(QPoint(quad.originalLeft(), quad.originalTop()))) {
                    bottomList.append(quad);
                    continue;
                }
                if (leftRect.contains(QPoint(quad.originalLeft(), quad.originalTop()))) {
                    leftList.append(quad);
                    continue;
                }
                if (rightRect.contains(QPoint(quad.originalLeft(), quad.originalTop()))) {
                    rightList.append(quad);
                    continue;
                }
            }

            paintDecoration(top, DecorationTop, region, topRect, data, topList, updateDeco, hardwareClipping);
            paintDecoration(left, DecorationLeft, region, leftRect, data, leftList, updateDeco, hardwareClipping);
            paintDecoration(right, DecorationRight, region, rightRect, data, rightList, updateDeco, hardwareClipping);
            paintDecoration(bottom, DecorationBottom, region, bottomRect, data, bottomList, updateDeco, hardwareClipping);
        }
    }

    // paint the content
    WindowQuadList contentQuads = data.quads.select(WindowQuadContents);
    if (!contentQuads.empty()) {
        texture.bind();
        prepareStates(Content, data.opacity(), data.brightness(), data.saturation(), data.screen(), data.shader);
        renderQuads(mask, region, contentQuads, &texture, false, hardwareClipping);
        restoreStates(Content, data.opacity(), data.brightness(), data.saturation(), data.screen(), data.shader);
        texture.unbind();
#ifndef KWIN_HAVE_OPENGLES
        if (m_scene && m_scene->debug) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            renderQuads(mask, region, contentQuads, &texture, false, hardwareClipping);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
#endif
    }

    if (hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }

    if (sceneShader) {
        ShaderManager::instance()->popShader();
        data.shader = NULL;
    } else {
        popMatrix();
    }
}

void SceneOpenGL::Window::paintDecoration(const QPixmap* decoration, TextureType decorationType,
                                          const QRegion& region, const QRect& rect, const WindowPaintData& data,
                                          const WindowQuadList& quads, bool updateDeco, bool hardwareClipping)
{
    SceneOpenGL::Texture* decorationTexture;
    switch(decorationType) {
    case DecorationTop:
        decorationTexture = &topTexture;
        break;
    case DecorationLeft:
        decorationTexture = &leftTexture;
        break;
    case DecorationRight:
        decorationTexture = &rightTexture;
        break;
    case DecorationBottom:
        decorationTexture = &bottomTexture;
        break;
    default:
        return;
    }
    if (decoration->isNull()) {
        return;
    }
    if (decorationTexture->isNull() || updateDeco) {
        bool success = decorationTexture->load(*decoration);
        if (!success) {
            kDebug(1212) << "Failed to bind decoartion";
            return;
        }
    }

    // We have to update the texture although we do not paint anything.
    // This is especially needed if we draw the opaque part of the window
    // and the decoration in two different passes (as we in Scene::paintSimpleWindow do).
    // Otherwise we run into the situation that in the first pass there are some
    // pending decoration repaints but we don't paint the decoration and in the
    // second pass it's the other way around.
    if (quads.isEmpty())
        return;

    if (filter == ImageFilterGood)
        decorationTexture->setFilter(GL_LINEAR);
    else
        decorationTexture->setFilter(GL_NEAREST);
    decorationTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    decorationTexture->bind();

    prepareStates(decorationType, data.opacity() * data.decorationOpacity(), data.brightness(), data.saturation(), data.screen(), data.shader);
    makeDecorationArrays(quads, rect, decorationTexture);
    GLVertexBuffer::streamingBuffer()->render(region, GL_TRIANGLES, hardwareClipping);
    restoreStates(decorationType, data.opacity() * data.decorationOpacity(), data.brightness(), data.saturation(), data.screen(), data.shader);
    decorationTexture->unbind();
#ifndef KWIN_HAVE_OPENGLES
    if (m_scene && m_scene->debug) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GLVertexBuffer::streamingBuffer()->render(region, GL_TRIANGLES, hardwareClipping);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
}

void SceneOpenGL::Window::paintShadow(const QRegion &region, const WindowPaintData &data, bool hardwareClipping)
{
    WindowQuadList quads = data.quads.select(WindowQuadShadowTopLeft);
    quads.append(data.quads.select(WindowQuadShadowTop));
    quads.append(data.quads.select(WindowQuadShadowTopRight));
    quads.append(data.quads.select(WindowQuadShadowRight));
    quads.append(data.quads.select(WindowQuadShadowBottomRight));
    quads.append(data.quads.select(WindowQuadShadowBottom));
    quads.append(data.quads.select(WindowQuadShadowBottomLeft));
    quads.append(data.quads.select(WindowQuadShadowLeft));
    if (quads.isEmpty()) {
        return;
    }
    GLTexture *texture = static_cast<SceneOpenGLShadow*>(m_shadow)->shadowTexture();
    if (!texture) {
        return;
    }
    if (filter == ImageFilterGood)
        texture->setFilter(GL_LINEAR);
    else
        texture->setFilter(GL_NEAREST);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->bind();
    prepareStates(Shadow, data.opacity(), data.brightness(), data.saturation(), data.screen(), data.shader, texture);
    renderQuads(0, region, quads, texture, true, hardwareClipping);
    restoreStates(Shadow, data.opacity(), data.brightness(), data.saturation(), data.screen(), data.shader, texture);
    texture->unbind();
#ifndef KWIN_HAVE_OPENGLES
    if (m_scene && m_scene->debug) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        renderQuads(0, region, quads, texture, true, hardwareClipping);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
}

void SceneOpenGL::Window::makeDecorationArrays(const WindowQuadList& quads, const QRect &rect, Texture *tex) const
{
    QVector<float> vertices;
    QVector<float> texcoords;
    vertices.reserve(quads.count() * 6 * 2);
    texcoords.reserve(quads.count() * 6 * 2);
    float width = rect.width();
    float height = rect.height();
#ifndef KWIN_HAVE_OPENGLES
    if (tex->target() == GL_TEXTURE_RECTANGLE_ARB) {
        width = 1.0;
        height = 1.0;
    }
#endif
    foreach (const WindowQuad & quad, quads) {
        vertices << quad[ 1 ].x();
        vertices << quad[ 1 ].y();
        vertices << quad[ 0 ].x();
        vertices << quad[ 0 ].y();
        vertices << quad[ 3 ].x();
        vertices << quad[ 3 ].y();
        vertices << quad[ 3 ].x();
        vertices << quad[ 3 ].y();
        vertices << quad[ 2 ].x();
        vertices << quad[ 2 ].y();
        vertices << quad[ 1 ].x();
        vertices << quad[ 1 ].y();

        if (tex->isYInverted()) {
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << (float)(quad.originalTop() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << (float)(quad.originalTop() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << (float)(quad.originalTop() - rect.y()) / height;
        } else {
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalTop() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalTop() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalLeft() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalBottom() - rect.y()) / height;
            texcoords << (float)(quad.originalRight() - rect.x()) / width;
            texcoords << 1.0f - (float)(quad.originalTop() - rect.y()) / height;
        }
    }
    GLVertexBuffer::streamingBuffer()->setData(quads.count() * 6, 2, vertices.data(), texcoords.data());
}

void SceneOpenGL::Window::renderQuads(int, const QRegion& region, const WindowQuadList& quads,
                                      GLTexture *tex, bool normalized, bool hardwareClipping)
{
    if (quads.isEmpty())
        return;
    // Render geometry
    float* vertices;
    float* texcoords;
    QSizeF size(tex->size());
    if (normalized) {
        size.setWidth(1.0);
        size.setHeight(1.0);
    }
#ifndef KWIN_HAVE_OPENGLES
    if (tex->target() == GL_TEXTURE_RECTANGLE_ARB) {
        size.setWidth(1.0);
        size.setHeight(1.0);
    }
#endif
    quads.makeArrays(&vertices, &texcoords, size, tex->isYInverted());
    GLVertexBuffer::streamingBuffer()->setData(quads.count() * 6, 2, vertices, texcoords);
    GLVertexBuffer::streamingBuffer()->render(region, GL_TRIANGLES, hardwareClipping);
    delete[] vertices;
    delete[] texcoords;
}

void SceneOpenGL::Window::prepareStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader)
{
    if (shader)
        prepareShaderRenderStates(type, opacity, brightness, saturation, screen, shader);
    else {
        Texture *tex = NULL;
        switch(type) {
        case Content:
            tex = &texture;
            break;
        case DecorationTop:
            tex = &topTexture;
            break;
        case DecorationLeft:
            tex = &leftTexture;
            break;
        case DecorationRight:
            tex = &rightTexture;
            break;
        case DecorationBottom:
            tex = &bottomTexture;
            break;
        default:
            return;
        }
        prepareStates(type, opacity, brightness, saturation, screen, shader, tex);
    }
}

void SceneOpenGL::Window::prepareStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader, GLTexture *texture)
{
    if (shader) {
        prepareShaderRenderStates(type, opacity, brightness, saturation, screen, shader);
    } else {
        prepareRenderStates(type, opacity, brightness, saturation, screen, texture);
    }
}

void SceneOpenGL::Window::prepareShaderRenderStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader)
{
    // setup blending of transparent windows
    bool opaque = isOpaque() && opacity == 1.0;
    bool alpha = toplevel->hasAlpha() || type != Content;
    if (type != Content)
        opaque = false;
    if (!opaque) {
        glEnable(GL_BLEND);
        if (!options->isColorCorrected()) {
            if (alpha) {
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            } else {
                glBlendColor((float)opacity, (float)opacity, (float)opacity, (float)opacity);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_CONSTANT_ALPHA);
            }
        } else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    const float rgb = brightness * opacity;
    const float a = opacity;

    shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::Saturation,         saturation);
    shader->setUniform(GLShader::AlphaToOne,         opaque ? 1 : 0);

    m_scene->colorCorrection()->setupForOutput(screen);
}

void SceneOpenGL::Window::prepareRenderStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLTexture *tex)
{
    Q_UNUSED(screen)
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(type)
    Q_UNUSED(opacity)
    Q_UNUSED(brightness)
    Q_UNUSED(saturation)
    Q_UNUSED(tex)
#else
    bool alpha = false;
    bool opaque = true;
    if (type == Content) {
        alpha = toplevel->hasAlpha();
        opaque = isOpaque() && opacity == 1.0;
    } else {
        alpha = true;
        opaque = false;
    }
    // setup blending of transparent windows
    glPushAttrib(GL_ENABLE_BIT);
    if (!opaque) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (saturation != 1.0 && tex->saturationSupported()) {
        // First we need to get the color from [0; 1] range to [0.5; 1] range
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        const float scale_constant[] = { 1.0, 1.0, 1.0, 0.5};
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, scale_constant);
        tex->bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture(GL_TEXTURE1);
        float saturation_constant[] = { 0.5 + 0.5 * 0.30, 0.5 + 0.5 * 0.59, 0.5 + 0.5 * 0.11, saturation };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        tex->bind();

        // Finally we need to interpolate between the original image and the
        //  greyscale image to get wanted level of saturation
        glActiveTexture(GL_TEXTURE2);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        // Also replace alpha by primary color's alpha here
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        // And make primary color contain the wanted opacity
        glColor4f(opacity, opacity, opacity, opacity);
        tex->bind();

        if (alpha || brightness != 1.0f) {
            glActiveTexture(GL_TEXTURE3);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            // The color has to be multiplied by both opacity and brightness
            float opacityByBrightness = opacity * brightness;
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity);
            if (alpha) {
                // Multiply original texture's alpha by our opacity
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            } else {
                // Alpha will be taken from previous stage
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            }
            tex->bind();
        }

        glActiveTexture(GL_TEXTURE0);
    } else if (opacity != 1.0 || brightness != 1.0) {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        float opacityByBrightness = opacity * brightness;
        if (alpha) {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness,
                      opacity);
        } else {
            // Multiply color by brightness and replace alpha by opacity
            float constant[] = { opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity };
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);
        }
    } else if (!alpha && opaque) {
        float constant[] = { 1.0, 1.0, 1.0, 1.0 };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);
    }
#endif
}

void SceneOpenGL::Window::restoreStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader)
{
    if (shader)
        restoreShaderRenderStates(type, opacity, brightness, saturation, screen, shader);
    else {
        Texture *tex = NULL;
        switch(type) {
        case Content:
            tex = &texture;
            break;
        case DecorationTop:
            tex = &topTexture;
            break;
        case DecorationLeft:
            tex = &leftTexture;
            break;
        case DecorationRight:
            tex = &rightTexture;
            break;
        case DecorationBottom:
            tex = &bottomTexture;
            break;
        default:
            return;
        }
        restoreStates(type, opacity, brightness, saturation, screen, shader, tex);
    }
}

void SceneOpenGL::Window::restoreStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader, GLTexture *texture)
{
    if (shader) {
        restoreShaderRenderStates(type, opacity, brightness, saturation, screen, shader);
    } else {
        restoreRenderStates(type, opacity, brightness, saturation, screen, texture);
    }
}

void SceneOpenGL::Window::restoreShaderRenderStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLShader* shader)
{
    Q_UNUSED(brightness);
    Q_UNUSED(saturation);
    Q_UNUSED(screen)
    Q_UNUSED(shader);
    bool opaque = isOpaque() && opacity == 1.0;
    if (type != Content)
        opaque = false;
    if (!opaque) {
        glDisable(GL_BLEND);
    }
    ShaderManager::instance()->getBoundShader()->setUniform(GLShader::AlphaToOne, 0);
}

void SceneOpenGL::Window::restoreRenderStates(TextureType type, double opacity, double brightness, double saturation, int screen, GLTexture *tex)
{
    Q_UNUSED(type)
    Q_UNUSED(screen)
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(opacity)
    Q_UNUSED(brightness)
    Q_UNUSED(saturation)
    Q_UNUSED(tex)
#else
    if (opacity != 1.0 || saturation != 1.0 || brightness != 1.0f) {
        if (saturation != 1.0 && tex->saturationSupported()) {
            glActiveTexture(GL_TEXTURE3);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE2);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE1);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE0);
        }
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(0, 0, 0, 0);

    glPopAttrib();  // ENABLE_BIT
#endif
}

//****************************************
// SceneOpenGL::EffectFrame
//****************************************

SceneOpenGL::Texture* SceneOpenGL::EffectFrame::m_unstyledTexture = NULL;
QPixmap* SceneOpenGL::EffectFrame::m_unstyledPixmap = NULL;

SceneOpenGL::EffectFrame::EffectFrame(EffectFrameImpl* frame)
    : Scene::EffectFrame(frame)
    , m_texture(NULL)
    , m_textTexture(NULL)
    , m_oldTextTexture(NULL)
    , m_textPixmap(NULL)
    , m_iconTexture(NULL)
    , m_oldIconTexture(NULL)
    , m_selectionTexture(NULL)
    , m_unstyledVBO(NULL)
{
    if (m_effectFrame->style() == EffectFrameUnstyled && !m_unstyledTexture) {
        updateUnstyledTexture();
    }
}

SceneOpenGL::EffectFrame::~EffectFrame()
{
    delete m_texture;
    delete m_textTexture;
    delete m_textPixmap;
    delete m_oldTextTexture;
    delete m_iconTexture;
    delete m_oldIconTexture;
    delete m_selectionTexture;
    delete m_unstyledVBO;
}

void SceneOpenGL::EffectFrame::free()
{
    glFlush();
    delete m_texture;
    m_texture = NULL;
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
    delete m_iconTexture;
    m_iconTexture = NULL;
    delete m_selectionTexture;
    m_selectionTexture = NULL;
    delete m_unstyledVBO;
    m_unstyledVBO = NULL;
    delete m_oldIconTexture;
    m_oldIconTexture = NULL;
    delete m_oldTextTexture;
    m_oldTextTexture = NULL;
}

void SceneOpenGL::EffectFrame::freeIconFrame()
{
    delete m_iconTexture;
    m_iconTexture = NULL;
}

void SceneOpenGL::EffectFrame::freeTextFrame()
{
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
}

void SceneOpenGL::EffectFrame::freeSelection()
{
    delete m_selectionTexture;
    m_selectionTexture = NULL;
}

void SceneOpenGL::EffectFrame::crossFadeIcon()
{
    delete m_oldIconTexture;
    m_oldIconTexture = m_iconTexture;
    m_iconTexture = NULL;
}

void SceneOpenGL::EffectFrame::crossFadeText()
{
    delete m_oldTextTexture;
    m_oldTextTexture = m_textTexture;
    m_textTexture = NULL;
}

void SceneOpenGL::EffectFrame::render(QRegion region, double opacity, double frameOpacity)
{
    if (m_effectFrame->geometry().isEmpty())
        return; // Nothing to display

    region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    GLShader* shader = m_effectFrame->shader();
    bool sceneShader = false;
    if (!shader && ShaderManager::instance()->isValid()) {
        shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
        sceneShader = true;
    } else if (shader) {
        ShaderManager::instance()->pushShader(shader);
    }

    if (shader) {
        if (sceneShader)
            shader->setUniform(GLShader::Offset, QVector2D(0, 0));

        shader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));
        shader->setUniform(GLShader::Saturation, 1.0f);
        shader->setUniform(GLShader::AlphaToOne, 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef KWIN_HAVE_OPENGLES
    if (!shader)
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

    // Render the actual frame
    if (m_effectFrame->style() == EffectFrameUnstyled) {
        if (!m_unstyledVBO) {
            m_unstyledVBO = new GLVertexBuffer(GLVertexBuffer::Static);
            QRect area = m_effectFrame->geometry();
            area.moveTo(0, 0);
            area.adjust(-5, -5, 5, 5);

            const int roundness = 5;
            QVector<float> verts, texCoords;
            verts.reserve(84);
            texCoords.reserve(84);

            // top left
            verts << area.left() << area.top();
            texCoords << 0.0f << 0.0f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top right
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            // bottom left
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom right
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom();
            texCoords << 1.0f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            // center
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;

            m_unstyledVBO->setData(verts.count() / 2, 2, verts.data(), texCoords.data());
        }

        if (shader) {
            const float a = opacity * frameOpacity;
            shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
        }
#ifndef KWIN_HAVE_OPENGLES
        else
            glColor4f(0.0, 0.0, 0.0, opacity * frameOpacity);
#endif

        m_unstyledTexture->bind();
        const QPoint pt = m_effectFrame->geometry().topLeft();
        if (sceneShader) {
            shader->setUniform(GLShader::Offset, QVector2D(pt.x(), pt.y()));
        } else {
            QMatrix4x4 translation;
            translation.translate(pt.x(), pt.y());
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, translation);
            } else {
                pushMatrix(translation);
            }
        }
        m_unstyledVBO->render(region, GL_TRIANGLES);
        if (!sceneShader) {
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
            } else {
                popMatrix();
            }
        }
        m_unstyledTexture->unbind();
    } else if (m_effectFrame->style() == EffectFrameStyled) {
        if (!m_texture)   // Lazy creation
            updateTexture();

        if (shader) {
            const float a = opacity * frameOpacity;
            shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
        }
#ifndef KWIN_HAVE_OPENGLES
        else
            glColor4f(1.0, 1.0, 1.0, opacity * frameOpacity);
#endif
        m_texture->bind();
        qreal left, top, right, bottom;
        m_effectFrame->frame().getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        m_texture->render(region, m_effectFrame->geometry().adjusted(-left, -top, right, bottom));
        m_texture->unbind();

    }
    if (!m_effectFrame->selection().isNull()) {
        if (!m_selectionTexture) { // Lazy creation
            QPixmap pixmap = m_effectFrame->selectionFrame().framePixmap();
            if (!pixmap.isNull())
                m_selectionTexture = new Texture(pixmap);
        }
        if (m_selectionTexture) {
            if (shader) {
                const float a = opacity * frameOpacity;
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
    #ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity * frameOpacity);
    #endif
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            m_selectionTexture->bind();
            m_selectionTexture->render(region, m_effectFrame->selection());
            m_selectionTexture->unbind();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // Render icon
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
        QPoint topLeft(m_effectFrame->geometry().x(),
                       m_effectFrame->geometry().center().y() - m_effectFrame->iconSize().height() / 2);

        if (m_effectFrame->isCrossFade() && m_oldIconTexture) {
            if (shader) {
                const float a = opacity * (1.0 - m_effectFrame->crossFadeProgress());
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity * (1.0 - m_effectFrame->crossFadeProgress()));
#endif

            m_oldIconTexture->bind();
            m_oldIconTexture->render(region, QRect(topLeft, m_effectFrame->iconSize()));
            m_oldIconTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress());
#endif
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity);
#endif
        }

        if (!m_iconTexture) { // lazy creation
            m_iconTexture = new Texture(m_effectFrame->icon());
        }
        m_iconTexture->bind();
        m_iconTexture->render(region, QRect(topLeft, m_effectFrame->iconSize()));
        m_iconTexture->unbind();
    }

    // Render text
    if (!m_effectFrame->text().isEmpty()) {
        if (m_effectFrame->isCrossFade() && m_oldTextTexture) {
            if (shader) {
                const float a = opacity * (1.0 - m_effectFrame->crossFadeProgress());
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity *(1.0 - m_effectFrame->crossFadeProgress()));
#endif

            m_oldTextTexture->bind();
            m_oldTextTexture->render(region, m_effectFrame->geometry());
            m_oldTextTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress());
#endif
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f(1.0, 1.0, 1.0, opacity);
#endif
        }
        if (!m_textTexture)   // Lazy creation
            updateTextTexture();
        m_textTexture->bind();
        m_textTexture->render(region, m_effectFrame->geometry());
        m_textTexture->unbind();
    }

    if (shader) {
        ShaderManager::instance()->popShader();
    }
    glDisable(GL_BLEND);
}

void SceneOpenGL::EffectFrame::updateTexture()
{
    delete m_texture;
    m_texture = 0L;
    if (m_effectFrame->style() == EffectFrameStyled) {
        QPixmap pixmap = m_effectFrame->frame().framePixmap();
        m_texture = new Texture(pixmap);
    }
}

void SceneOpenGL::EffectFrame::updateTextTexture()
{
    delete m_textTexture;
    m_textTexture = 0L;
    delete m_textPixmap;
    m_textPixmap = 0L;

    if (m_effectFrame->text().isEmpty())
        return;

    // Determine position on texture to paint text
    QRect rect(QPoint(0, 0), m_effectFrame->geometry().size());
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty())
        rect.setLeft(m_effectFrame->iconSize().width());

    // If static size elide text as required
    QString text = m_effectFrame->text();
    if (m_effectFrame->isStatic()) {
        QFontMetrics metrics(m_effectFrame->font());
        text = metrics.elidedText(text, Qt::ElideRight, rect.width());
    }

    m_textPixmap = new QPixmap(m_effectFrame->geometry().size());
    m_textPixmap->fill(Qt::transparent);
    QPainter p(m_textPixmap);
    p.setFont(m_effectFrame->font());
    if (m_effectFrame->style() == EffectFrameStyled)
        p.setPen(m_effectFrame->styledTextColor());
    else // TODO: What about no frame? Custom color setting required
        p.setPen(Qt::white);
    p.drawText(rect, m_effectFrame->alignment(), text);
    p.end();
    m_textTexture = new Texture(*m_textPixmap);
}

void SceneOpenGL::EffectFrame::updateUnstyledTexture()
{
    delete m_unstyledTexture;
    m_unstyledTexture = 0L;
    delete m_unstyledPixmap;
    m_unstyledPixmap = 0L;
    // Based off circle() from kwinxrenderutils.cpp
#define CS 8
    m_unstyledPixmap = new QPixmap(2 * CS, 2 * CS);
    m_unstyledPixmap->fill(Qt::transparent);
    QPainter p(m_unstyledPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawEllipse(m_unstyledPixmap->rect());
    p.end();
#undef CS
    m_unstyledTexture = new Texture(*m_unstyledPixmap);
}

void SceneOpenGL::EffectFrame::cleanup()
{
    delete m_unstyledTexture;
    m_unstyledTexture = NULL;
    delete m_unstyledPixmap;
    m_unstyledPixmap = NULL;
}

//****************************************
// SceneOpenGL::Shadow
//****************************************
SceneOpenGLShadow::SceneOpenGLShadow(Toplevel *toplevel)
    : Shadow(toplevel)
    , m_texture(NULL)
{
}

SceneOpenGLShadow::~SceneOpenGLShadow()
{
    delete m_texture;
}

void SceneOpenGLShadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QSizeF top(shadowPixmap(ShadowElementTop).size());
    const QSizeF topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSizeF right(shadowPixmap(ShadowElementRight).size());
    const QSizeF bottomRight(shadowPixmap(ShadowElementBottomRight).size());
    const QSizeF bottom(shadowPixmap(ShadowElementBottom).size());
    const QSizeF bottomLeft(shadowPixmap(ShadowElementBottomLeft).size());
    const QSizeF left(shadowPixmap(ShadowElementLeft).size());
    const QSizeF topLeft(shadowPixmap(ShadowElementTopLeft).size());
    if ((left.width() - leftOffset() > topLevel()->width()) ||
        (right.width() - rightOffset() > topLevel()->width()) ||
        (top.height() - topOffset() > topLevel()->height()) ||
        (bottom.height() - bottomOffset() > topLevel()->height())) {
        // if our shadow is bigger than the window, we don't render the shadow
        setShadowRegion(QRegion());
        return;
    }

    const QRectF outerRect(QPointF(-leftOffset(), -topOffset()),
                           QPointF(topLevel()->width() + rightOffset(), topLevel()->height() + bottomOffset()));

    const qreal width = topLeft.width() + top.width() + topRight.width();
    const qreal height = topLeft.height() + left.height() + bottomLeft.height();

    qreal tx1(0.0), tx2(0.0), ty1(0.0), ty2(0.0);

    tx2 = topLeft.width()/width;
    ty2 = topLeft.height()/height;
    WindowQuad topLeftQuad(WindowQuadShadowTopLeft);
    topLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                      outerRect.y(), tx1, ty1);
    topLeftQuad[ 1 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y(), tx2, ty1);
    topLeftQuad[ 2 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y() + topLeft.height(), tx2, ty2);
    topLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                      outerRect.y() + topLeft.height(), tx1, ty2);
    m_shadowQuads.append(topLeftQuad);

    tx1 = tx2;
    tx2 = (topLeft.width() + top.width())/width;
    ty2 = top.height()/height;
    WindowQuad topQuad(WindowQuadShadowTop);
    topQuad[ 0 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y(), tx1, ty1);
    topQuad[ 1 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y(), tx2, ty1);
    topQuad[ 2 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y() + top.height(),tx2, ty2);
    topQuad[ 3 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y() + top.height(), tx1, ty2);
    m_shadowQuads.append(topQuad);

    tx1 = tx2;
    tx2 = 1.0;
    ty2 = topRight.height()/height;
    WindowQuad topRightQuad(WindowQuadShadowTopRight);
    topRightQuad[ 0 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y(), tx1, ty1);
    topRightQuad[ 1 ] = WindowVertex(outerRect.right(),                     outerRect.y(), tx2, ty1);
    topRightQuad[ 2 ] = WindowVertex(outerRect.right(),                     outerRect.y() + topRight.height(), tx2, ty2);
    topRightQuad[ 3 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y() + topRight.height(), tx1, ty2);
    m_shadowQuads.append(topRightQuad);

    tx1 = (width - right.width())/width;
    ty1 = topRight.height()/height;
    ty2 = (topRight.height() + right.height())/height;
    WindowQuad rightQuad(WindowQuadShadowRight);
    rightQuad[ 0 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.y() + topRight.height(), tx1, ty1);
    rightQuad[ 1 ] = WindowVertex(outerRect.right(),                    outerRect.y() + topRight.height(), tx2, ty1);
    rightQuad[ 2 ] = WindowVertex(outerRect.right(),                    outerRect.bottom() - bottomRight.height(), tx2, ty2);
    rightQuad[ 3 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty2);
    m_shadowQuads.append(rightQuad);

    tx1 = (width - bottomRight.width())/width;
    ty1 = ty2;
    ty2 = 1.0;
    WindowQuad bottomRightQuad(WindowQuadShadowBottomRight);
    bottomRightQuad[ 0 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty1);
    bottomRightQuad[ 1 ] = WindowVertex(outerRect.right(),                          outerRect.bottom() - bottomRight.height(), tx2, ty1);
    bottomRightQuad[ 2 ] = WindowVertex(outerRect.right(),                          outerRect.bottom(), tx2, ty2);
    bottomRightQuad[ 3 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomRightQuad);

    tx2 = tx1;
    tx1 = bottomLeft.width()/width;
    ty1 = (height - bottom.height())/height;
    WindowQuad bottomQuad(WindowQuadShadowBottom);
    bottomQuad[ 0 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom() - bottom.height(), tx1, ty1);
    bottomQuad[ 1 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom() - bottom.height(), tx2, ty1);
    bottomQuad[ 2 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom(), tx2, ty2);
    bottomQuad[ 3 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomQuad);

    tx1 = 0.0;
    tx2 = bottomLeft.width()/width;
    ty1 = (height - bottomLeft.height())/height;
    WindowQuad bottomLeftQuad(WindowQuadShadowBottomLeft);
    bottomLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                       outerRect.bottom() - bottomLeft.height(), tx1, ty1);
    bottomLeftQuad[ 1 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty1);
    bottomLeftQuad[ 2 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom(), tx2, ty2);
    bottomLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                       outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomLeftQuad);

    tx2 = left.width()/width;
    ty2 = ty1;
    ty1 = topLeft.height()/height;
    WindowQuad leftQuad(WindowQuadShadowLeft);
    leftQuad[ 0 ] = WindowVertex(outerRect.x(),                 outerRect.y() + topLeft.height(), tx1, ty1);
    leftQuad[ 1 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.y() + topLeft.height(), tx2, ty1);
    leftQuad[ 2 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty2);
    leftQuad[ 3 ] = WindowVertex(outerRect.x(),                 outerRect.bottom() - bottomLeft.height(), tx1, ty2);
    m_shadowQuads.append(leftQuad);
}

bool SceneOpenGLShadow::prepareBackend()
{
    const QSize top(shadowPixmap(ShadowElementTop).size());
    const QSize topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSize right(shadowPixmap(ShadowElementRight).size());
    const QSize bottomRight(shadowPixmap(ShadowElementBottomRight).size());
    const QSize bottom(shadowPixmap(ShadowElementBottom).size());
    const QSize bottomLeft(shadowPixmap(ShadowElementBottomLeft).size());
    const QSize left(shadowPixmap(ShadowElementLeft).size());
    const QSize topLeft(shadowPixmap(ShadowElementTopLeft).size());

    const int width = topLeft.width() + top.width() + topRight.width();
    const int height = topLeft.height() + left.height() + bottomLeft.height();

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    p.drawPixmap(0, 0, shadowPixmap(ShadowElementTopLeft));
    p.drawPixmap(topLeft.width(), 0, shadowPixmap(ShadowElementTop));
    p.drawPixmap(topLeft.width() + top.width(), 0, shadowPixmap(ShadowElementTopRight));
    p.drawPixmap(0, topLeft.height(), shadowPixmap(ShadowElementLeft));
    p.drawPixmap(width - right.width(), topRight.height(), shadowPixmap(ShadowElementRight));
    p.drawPixmap(0, topLeft.height() + left.height(), shadowPixmap(ShadowElementBottomLeft));
    p.drawPixmap(bottomLeft.width(), height - bottom.height(), shadowPixmap(ShadowElementBottom));
    p.drawPixmap(bottomLeft.width() + bottom.width(), topRight.height() + right.height(), shadowPixmap(ShadowElementBottomRight));
    p.end();

    delete m_texture;
    m_texture = new GLTexture(image);

    return true;
}

} // namespace
