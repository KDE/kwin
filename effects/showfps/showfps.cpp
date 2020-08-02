/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfps.h"

// KConfigSkeleton
#include "showfpsconfig.h"

#include <kwinconfig.h>

#include <kwinglutils.h>
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <kwinxrenderutils.h>
#include <xcb/render.h>
#endif

#include <KLocalizedString>

#include <QPainter>
#include <QVector2D>
#include <QPalette>

#include <cmath>

namespace KWin
{

const int FPS_WIDTH = 10;
const int MAX_TIME = 100;

ShowFpsEffect::ShowFpsEffect()
    : paints_pos(0)
    , frames_pos(0)
    , m_noBenchmark(effects->effectFrame(EffectFrameUnstyled, false))
{
    initConfig<ShowFpsConfig>();
    for (int i = 0;
            i < NUM_PAINTS;
            ++i) {
        paints[ i ] = 0;
        paint_size[ i ] = 0;
    }
    for (int i = 0;
            i < MAX_FPS;
            ++i)
        frames[ i ] = 0;
    m_noBenchmark->setAlignment(Qt::AlignTop | Qt::AlignRight);
    m_noBenchmark->setText(i18n("This effect is not a benchmark"));
    reconfigure(ReconfigureAll);
}

void ShowFpsEffect::reconfigure(ReconfigureFlags)
{
    ShowFpsConfig::self()->read();
    alpha = ShowFpsConfig::alpha();
    x = ShowFpsConfig::x();
    y = ShowFpsConfig::y();
    const QSize screenSize = effects->virtualScreenSize();
    if (x == -10000)   // there's no -0 :(
        x = screenSize.width() - 2 * NUM_PAINTS - FPS_WIDTH;
    else if (x < 0)
        x = screenSize.width() - 2 * NUM_PAINTS - FPS_WIDTH - x;
    if (y == -10000)
        y = screenSize.height() - MAX_TIME;
    else if (y < 0)
        y = screenSize.height() - MAX_TIME - y;
    fps_rect = QRect(x, y, FPS_WIDTH + 2 * NUM_PAINTS, MAX_TIME);
    m_noBenchmark->setPosition(fps_rect.bottomRight() + QPoint(-6, 6));

    int textPosition = ShowFpsConfig::textPosition();
    textFont = ShowFpsConfig::textFont();
    textColor = ShowFpsConfig::textColor();
    double textAlpha = ShowFpsConfig::textAlpha();

    if (!textColor.isValid())
        textColor = QPalette().color(QPalette::Active, QPalette::WindowText);
    textColor.setAlphaF(textAlpha);

    switch(textPosition) {
    case TOP_LEFT:
        fpsTextRect = QRect(0, 0, 100, 100);
        textAlign = Qt::AlignTop | Qt::AlignLeft;
        break;
    case TOP_RIGHT:
        fpsTextRect = QRect(screenSize.width() - 100, 0, 100, 100);
        textAlign = Qt::AlignTop | Qt::AlignRight;
        break;
    case BOTTOM_LEFT:
        fpsTextRect = QRect(0, screenSize.height() - 100, 100, 100);
        textAlign = Qt::AlignBottom | Qt::AlignLeft;
        break;
    case BOTTOM_RIGHT:
        fpsTextRect = QRect(screenSize.width() - 100, screenSize.height() - 100, 100, 100);
        textAlign = Qt::AlignBottom | Qt::AlignRight;
        break;
    case NOWHERE:
        fpsTextRect = QRect();
        break;
    case INSIDE_GRAPH:
    default:
        fpsTextRect = QRect(x, y, FPS_WIDTH + NUM_PAINTS, MAX_TIME);
        textAlign = Qt::AlignTop | Qt::AlignRight;
        break;
    }
}

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    frames[ frames_pos ] = QDateTime::currentMSecsSinceEpoch();
    if (++frames_pos == MAX_FPS)
        frames_pos = 0;
    effects->prePaintScreen(data, time);
    data.paint += fps_rect;

    paint_size[ paints_pos ] = 0;
    t.restart();
}

void ShowFpsEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->paintWindow(w, mask, region, data);

    // Take intersection of region and actual window's rect, minus the fps area
    //  (since we keep repainting it) and count the pixels.
    QRegion r2 = region & QRect(w->x(), w->y(), w->width(), w->height());
    r2 -= fps_rect;
    int winsize = 0;
    for (const QRect &r : r2) {
        winsize += r.width() * r.height();
    }
    paint_size[ paints_pos ] += winsize;
}

void ShowFpsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
    int lastFrame = frames_pos - 1;
    if (lastFrame < 0)
        lastFrame = MAX_FPS - 1;
    const qint64 lastTimestamp = frames[lastFrame];
    int fps = 0;
    for (int i = 0;
            i < MAX_FPS;
            ++i)
        if (abs(lastTimestamp - frames[ i ]) < 1000)
            ++fps; // count all frames in the last second
    if (fps > MAX_TIME)
        fps = MAX_TIME; // keep it the same height
    if (effects->isOpenGLCompositing()) {
        paintGL(fps, data.projectionMatrix());
        glFinish(); // make sure all rendering is done
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing) {
        paintXrender(fps);
        xcb_flush(xcbConnection());   // make sure all rendering is done
    }
#endif
    if (effects->compositingType() == QPainterCompositing) {
        paintQPainter(fps);
    }
    m_noBenchmark->render(infiniteRegion(), 1.0, alpha);
}

void ShowFpsEffect::paintGL(int fps, const QMatrix4x4 &projectionMatrix)
{
    int x = this->x;
    int y = this->y;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // TODO painting first the background white and then the contents
    // means that the contents also blend with the background, I guess
    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    QColor color(255, 255, 255);
    color.setAlphaF(alpha);
    vbo->setColor(color);
    QVector<float> verts;
    verts.reserve(12);
    verts << x + 2 * NUM_PAINTS + FPS_WIDTH << y;
    verts << x << y;
    verts << x << y + MAX_TIME;
    verts << x << y + MAX_TIME;
    verts << x + 2 * NUM_PAINTS + FPS_WIDTH << y + MAX_TIME;
    verts << x + 2 * NUM_PAINTS + FPS_WIDTH << y;
    vbo->setData(6, 2, verts.constData(), nullptr);
    vbo->render(GL_TRIANGLES);
    y += MAX_TIME; // paint up from the bottom
    color.setRed(0);
    color.setGreen(0);
    vbo->setColor(color);
    verts.clear();
    verts << x + FPS_WIDTH << y - fps;
    verts << x << y - fps;
    verts << x << y;
    verts << x << y;
    verts << x + FPS_WIDTH << y;
    verts << x + FPS_WIDTH << y - fps;
    vbo->setData(6, 2, verts.constData(), nullptr);
    vbo->render(GL_TRIANGLES);


    color.setBlue(0);
    vbo->setColor(color);
    QVector<float> vertices;
    for (int i = 10;
            i < MAX_TIME;
            i += 10) {
        vertices << x << y - i;
        vertices << x + FPS_WIDTH << y - i;
    }
    vbo->setData(vertices.size() / 2, 2, vertices.constData(), nullptr);
    vbo->render(GL_LINES);
    x += FPS_WIDTH;

    // Paint FPS graph
    paintFPSGraph(x, y);
    x += NUM_PAINTS;

    // Paint amount of rendered pixels graph
    paintDrawSizeGraph(x, y);

    // Paint FPS numerical value
    if (fpsTextRect.isValid()) {
        fpsText.reset(new GLTexture(fpsTextImage(fps)));
        fpsText->bind();
        ShaderBinder binder(ShaderTrait::MapTexture);
        QMatrix4x4 mvp = projectionMatrix;
        mvp.translate(fpsTextRect.x(), fpsTextRect.y());
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        fpsText->render(QRegion(fpsTextRect), fpsTextRect);
        fpsText->unbind();
        effects->addRepaint(fpsTextRect);
    }

    // Paint paint sizes
    glDisable(GL_BLEND);
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
/*
 Differences between OpenGL and XRender:
 - differently specified rectangles (X: width/height, O: x2,y2)
 - XRender uses pre-multiplied alpha
*/
void ShowFpsEffect::paintXrender(int fps)
{
    xcb_pixmap_t pixmap = xcb_generate_id(xcbConnection());
    xcb_create_pixmap(xcbConnection(), 32, pixmap, x11RootWindow(), FPS_WIDTH, MAX_TIME);
    XRenderPicture p(pixmap, 32);
    xcb_free_pixmap(xcbConnection(), pixmap);
    xcb_render_color_t col;
    col.alpha = int(alpha * 0xffff);
    col.red = int(alpha * 0xffff);   // white
    col.green = int(alpha * 0xffff);
    col.blue = int(alpha * 0xffff);
    xcb_rectangle_t rect = {0, 0, FPS_WIDTH, MAX_TIME};
    xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, 1, &rect);
    col.red = 0; // blue
    col.green = 0;
    col.blue = int(alpha * 0xffff);
    rect.y = MAX_TIME - fps;
    rect.width = FPS_WIDTH;
    rect.height = fps;
    xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, 1, &rect);
    col.red = 0; // black
    col.green = 0;
    col.blue = 0;
    QVector<xcb_rectangle_t> rects;
    for (int i = 10;
            i < MAX_TIME;
            i += 10) {
        xcb_rectangle_t rect = {0, int16_t(MAX_TIME - i), uint16_t(FPS_WIDTH), 1};
        rects << rect;
    }
    xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, rects.count(), rects.constData());
    xcb_render_composite(xcbConnection(), alpha != 1.0 ? XCB_RENDER_PICT_OP_OVER : XCB_RENDER_PICT_OP_SRC, p, XCB_RENDER_PICTURE_NONE,
                         effects->xrenderBufferPicture(), 0, 0, 0, 0, x, y, FPS_WIDTH, MAX_TIME);


    // Paint FPS graph
    paintFPSGraph(x + FPS_WIDTH, y);

    // Paint amount of rendered pixels graph
    paintDrawSizeGraph(x + FPS_WIDTH + MAX_TIME, y);

    // Paint FPS numerical value
    if (fpsTextRect.isValid()) {
        QImage textImg(fpsTextImage(fps));
        XRenderPicture textPic(textImg);
        xcb_render_composite(xcbConnection(), XCB_RENDER_PICT_OP_OVER, textPic, XCB_RENDER_PICTURE_NONE,
                        effects->xrenderBufferPicture(), 0, 0, 0, 0, fpsTextRect.x(), fpsTextRect.y(), textImg.width(), textImg.height());
        effects->addRepaint(fpsTextRect);
    }
}
#endif

void ShowFpsEffect::paintQPainter(int fps)
{
    QPainter *painter = effects->scenePainter();
    painter->save();

    QColor color(255, 255, 255);
    color.setAlphaF(alpha);

    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->fillRect(x, y, 2 * NUM_PAINTS + FPS_WIDTH, MAX_TIME, color);
    color.setRed(0);
    color.setGreen(0);
    painter->fillRect(x, y + MAX_TIME - fps, FPS_WIDTH, fps, color);

    color.setBlue(0);
    for (int i = 10; i < MAX_TIME; i += 10) {
        painter->setPen(color);
        painter->drawLine(x, y + MAX_TIME - i, x + FPS_WIDTH, y + MAX_TIME - i);
    }

    // Paint FPS graph
    paintFPSGraph(x + FPS_WIDTH, y + MAX_TIME - 1);

    // Paint amount of rendered pixels graph
    paintDrawSizeGraph(x + FPS_WIDTH + NUM_PAINTS, y + MAX_TIME - 1);

    // Paint FPS numerical value
    painter->setPen(Qt::black);
    painter->drawText(fpsTextRect, textAlign, QString::number(fps));

    painter->restore();
}

void ShowFpsEffect::paintFPSGraph(int x, int y)
{
    // Paint FPS graph
    QList<int> lines;
    lines << 10 << 20 << 50;
    QList<int> values;
    for (int i = 0;
            i < NUM_PAINTS;
            ++i) {
        values.append(paints[(i + paints_pos) % NUM_PAINTS ]);
    }
    paintGraph(x, y, values, lines, true);
}

void ShowFpsEffect::paintDrawSizeGraph(int x, int y)
{
    int max_drawsize = 0;
    for (int i = 0; i < NUM_PAINTS; i++)
        max_drawsize = qMax(max_drawsize, paint_size[ i ]);

    // Log of min/max values shown on graph
    const float max_pixels_log = 7.2f;
    const float min_pixels_log = 2.0f;
    const int minh = 5;  // Minimum height of the bar when  value > 0

    float drawscale = (MAX_TIME - minh) / (max_pixels_log - min_pixels_log);
    QList<int> drawlines;

    for (int logh = (int)min_pixels_log; logh <= max_pixels_log; logh++)
        drawlines.append((int)((logh - min_pixels_log) * drawscale) + minh);

    QList<int> drawvalues;
    for (int i = 0;
            i < NUM_PAINTS;
            ++i) {
        int value = paint_size[(i + paints_pos) % NUM_PAINTS ];
        int h = 0;
        if (value > 0) {
            h = (int)((log10((double)value) - min_pixels_log) * drawscale);
            h = qMin(qMax(0, h) + minh, MAX_TIME);
        }
        drawvalues.append(h);
    }
    paintGraph(x, y, drawvalues, drawlines, false);
}

void ShowFpsEffect::paintGraph(int x, int y, QList<int> values, QList<int> lines, bool colorize)
{
    if (effects->isOpenGLCompositing()) {
        QColor color(0, 0, 0);
        color.setAlphaF(alpha);
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setColor(color);
        QVector<float> verts;
        // First draw the lines
        foreach (int h, lines) {
            verts << x << y - h;
            verts << x + values.count() << y - h;
        }
        vbo->setData(verts.size() / 2, 2, verts.constData(), nullptr);
        vbo->render(GL_LINES);
        // Then the graph values
        int lastValue = 0;
        verts.clear();
        for (int i = 0; i < values.count(); i++) {
            int value = values[ i ];
            if (colorize && value != lastValue) {
                if (!verts.isEmpty()) {
                    vbo->setData(verts.size() / 2, 2, verts.constData(), nullptr);
                    vbo->render(GL_LINES);
                }
                verts.clear();
                if (value <= 10) {
                    color = QColor(0, 255, 0);
                } else if (value <= 20) {
                    color = QColor(255, 255, 0);
                } else if (value <= 50) {
                    color = QColor(255, 0, 0);
                } else {
                    color = QColor(0, 0, 0);
                }
                vbo->setColor(color);
            }
            verts << x + values.count() - i << y;
            verts << x + values.count() - i << y - value;
            lastValue = value;
        }
        if (!verts.isEmpty()) {
            vbo->setData(verts.size() / 2, 2, verts.constData(), nullptr);
            vbo->render(GL_LINES);
        }
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing) {
        xcb_pixmap_t pixmap = xcb_generate_id(xcbConnection());
        xcb_create_pixmap(xcbConnection(), 32, pixmap, x11RootWindow(), values.count(), MAX_TIME);
        XRenderPicture p(pixmap, 32);
        xcb_free_pixmap(xcbConnection(), pixmap);
        xcb_render_color_t col;
        col.alpha = int(alpha * 0xffff);

        // Draw background
        col.red = col.green = col.blue = int(alpha * 0xffff);   // white
        xcb_rectangle_t rect = {0, 0, uint16_t(values.count()), uint16_t(MAX_TIME)};
        xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, 1, &rect);

        // Then the values
        col.red = col.green = col.blue = int(alpha * 0x8000);    // grey
        for (int i = 0; i < values.count(); i++) {
            int value = values[ i ];
            if (colorize) {
                if (value <= 10) {
                    // green
                    col.red = 0;
                    col.green = int(alpha * 0xffff);
                    col.blue = 0;
                } else if (value <= 20) {
                    // yellow
                    col.red = int(alpha * 0xffff);
                    col.green = int(alpha * 0xffff);
                    col.blue = 0;
                } else if (value <= 50) {
                    // red
                    col.red = int(alpha * 0xffff);
                    col.green = 0;
                    col.blue = 0;
                } else {
                    // black
                    col.red = 0;
                    col.green = 0;
                    col.blue = 0;
                }
            }
            xcb_rectangle_t rect = {int16_t(values.count() - i), int16_t(MAX_TIME - value), 1, uint16_t(value)};
            xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, 1, &rect);
        }

        // Then the lines
        col.red = col.green = col.blue = 0;  // black
        QVector<xcb_rectangle_t> rects;
        foreach (int h, lines) {
            xcb_rectangle_t rect = {0, int16_t(MAX_TIME - h), uint16_t(values.count()), 1};
            rects << rect;
        }
        xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, p, col, rects.count(), rects.constData());

        // Finally render the pixmap onto screen
        xcb_render_composite(xcbConnection(), alpha != 1.0 ? XCB_RENDER_PICT_OP_OVER : XCB_RENDER_PICT_OP_SRC, p,
                             XCB_RENDER_PICTURE_NONE, effects->xrenderBufferPicture(), 0, 0, 0, 0, x, y, values.count(), MAX_TIME);
    }
#endif
    if (effects->compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        painter->setPen(Qt::black);
        // First draw the lines
        foreach (int h, lines) {
            painter->drawLine(x, y - h, x + values.count(), y - h);
        }
        QColor color(0, 0, 0);
        color.setAlphaF(alpha);
        for (int i = 0; i < values.count(); i++) {
            int value = values[ i ];
            if (colorize) {
                if (value <= 10) {
                    color = QColor(0, 255, 0);
                } else if (value <= 20) {
                    color = QColor(255, 255, 0);
                } else if (value <= 50) {
                    color = QColor(255, 0, 0);
                } else {
                    color = QColor(0, 0, 0);
                }
            }
            painter->setPen(color);
            painter->drawLine(x + values.count() - i, y, x + values.count() - i, y - value);
        }
    }
}

void ShowFpsEffect::postPaintScreen()
{
    effects->postPaintScreen();
    paints[ paints_pos ] = t.elapsed();
    if (++paints_pos == NUM_PAINTS)
        paints_pos = 0;
    effects->addRepaint(fps_rect);
}

QImage ShowFpsEffect::fpsTextImage(int fps)
{
    QImage im(100, 100, QImage::Format_ARGB32);
    im.fill(Qt::transparent);
    QPainter painter(&im);
    painter.setFont(textFont);
    painter.setPen(textColor);
    painter.drawText(QRect(0, 0, 100, 100), textAlign, QString::number(fps));
    painter.end();
    return im;
}

} // namespace
