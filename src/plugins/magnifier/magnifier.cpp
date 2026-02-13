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
#include <QTimer>

#include <KStandardActions>

#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/eglcontext.h"
#include "opengl/glutils.h"
#include "utils/keys.h"
#include <KGlobalAccel>

using namespace std::chrono_literals;

namespace KWin
{

const int FRAME_WIDTH = 5;

MagnifierEffect::MagnifierEffect()
    : m_zoom(1)
    , m_targetZoom(1)
{
    m_configurationTimer = std::make_unique<QTimer>();
    m_configurationTimer->setSingleShot(true);
    m_configurationTimer->setInterval(1s);
    connect(m_configurationTimer.get(), &QTimer::timeout, this, &MagnifierEffect::saveInitialZoom);

    MagnifierConfig::instance(effects->config());
    QAction *a;
    a = KStandardActions::zoomIn(this, &MagnifierEffect::zoomIn, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));

    a = KStandardActions::zoomOut(this, &MagnifierEffect::zoomOut, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));

    a = KStandardActions::actualSize(this, &MagnifierEffect::toggle, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));

    m_touchpadAction = std::make_unique<QAction>();
    connect(m_touchpadAction.get(), &QAction::triggered, this, [this]() {
        const double threshold = 1.15;
        if (m_targetZoom < threshold) {
            setTargetZoom(1.0); // zoomTo
        }
        m_lastPinchProgress = 0;
    });
    effects->registerTouchpadPinchShortcut(PinchDirection::Expanding, 3, m_touchpadAction.get(), [this](qreal progress) {
        const qreal delta = progress - m_lastPinchProgress;
        m_lastPinchProgress = progress;
        realtimeZoom(delta);
    });
    effects->registerTouchpadPinchShortcut(PinchDirection::Contracting, 3, m_touchpadAction.get(), [this](qreal progress) {
        const qreal delta = progress - m_lastPinchProgress;
        m_lastPinchProgress = progress;
        realtimeZoom(-delta);
    });

    connect(effects, &EffectsHandler::mouseChanged, this, &MagnifierEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &MagnifierEffect::slotWindowAdded);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        slotWindowAdded(window);
    }

    reconfigure(ReconfigureAll);

    const double initialZoom = MagnifierConfig::initialZoom();
    if (initialZoom > 1.0) {
        setTargetZoom(initialZoom);
    }
}

MagnifierEffect::~MagnifierEffect()
{
    // Save the zoom value.
    saveInitialZoom();
}

bool MagnifierEffect::supported()
{
    return effects->openglContext() && effects->openglContext()->supportsBlits();
}

void MagnifierEffect::reconfigure(ReconfigureFlags)
{
    MagnifierConfig::self()->read();

    const QRect oldVisibleArea = visibleArea();

    int width, height;
    width = MagnifierConfig::width();
    height = MagnifierConfig::height();
    m_magnifierSize = QSize(width, height);
    m_zoomFactor = MagnifierConfig::zoomFactor();

    const Qt::KeyboardModifiers pointerAxisModifiers = stringToKeyboardModifiers(MagnifierConfig::pointerAxisGestureModifiers());
    if (m_axisModifiers != pointerAxisModifiers) {
        m_zoomInAxisAction.reset();
        m_zoomOutAxisAction.reset();
        m_axisModifiers = pointerAxisModifiers;

        if (pointerAxisModifiers) {
            m_zoomInAxisAction = std::make_unique<QAction>();
            connect(m_zoomInAxisAction.get(), &QAction::triggered, this, &MagnifierEffect::zoomIn);
            effects->registerAxisShortcut(pointerAxisModifiers, PointerAxisUp, m_zoomInAxisAction.get());

            m_zoomOutAxisAction = std::make_unique<QAction>();
            connect(m_zoomOutAxisAction.get(), &QAction::triggered, this, &MagnifierEffect::zoomOut);
            effects->registerAxisShortcut(pointerAxisModifiers, PointerAxisDown, m_zoomOutAxisAction.get());
        }
    }

    if (m_zoom > 1.0) {
        effects->addRepaint(oldVisibleArea.united(visibleArea()));
    }
}

void MagnifierEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const int time = m_clock.tick(data.view).count();

    if (m_zoom != m_targetZoom) {
        double diff = time / animationTime(500ms);
        if (m_targetZoom > m_zoom) {
            m_zoom = std::min(m_zoom * std::max(1 + diff, 1.2), m_targetZoom);
        } else {
            m_zoom = std::max(m_zoom * std::min(1 - diff, 0.8), m_targetZoom);
        }
    }
    if (m_zoom == 1.0) {
        // zoom ended - delete FBO and texture
        m_fbo.reset();
        m_texture.reset();
    } else if (!m_texture || m_texture->size() != m_magnifierSize) {
        if (auto texture = GLTexture::allocate(GL_RGBA16F, m_magnifierSize)) {
            texture->setWrapMode(GL_CLAMP_TO_EDGE);
            texture->setFilter(GL_LINEAR);

            if (auto fbo = std::make_unique<GLFramebuffer>(texture.get()); fbo->valid()) {
                m_texture = std::move(texture);
                m_fbo = std::move(fbo);
            }
        }
    }

    if (m_zoom == m_targetZoom) {
        m_clock.reset();
    }

    effects->prePaintScreen(data);
    if (m_zoom != 1.0) {
        data.paint += visibleArea();
    }
}

void MagnifierEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen); // paint normal screen
    if (m_zoom != 1.0 && m_fbo) {
        // get the right area from the current rendered screen
        const QRect area = magnifierArea();
        const QPointF cursor = cursorPos();
        const auto scale = viewport.scale();

        QRectF srcArea(cursor.x() - (double)area.width() / (m_zoom * 2),
                       cursor.y() - (double)area.height() / (m_zoom * 2),
                       (double)area.width() / m_zoom, (double)area.height() / m_zoom);
        if (effects->isOpenGLCompositing()) {
            m_fbo->blitFromRenderTarget(renderTarget, viewport, srcArea.toRect(), QRect(QPoint(), m_fbo->size()));
            // paint magnifier
            auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
            QMatrix4x4 mvp = viewport.projectionMatrix();
            mvp.translate(area.x() * scale, area.y() * scale);
            s->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
            m_texture->render(area.size() * scale);
            ShaderManager::instance()->popShader();

            GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();

            QRectF areaF = scaledRect(area, scale);
            const QRectF frame = scaledRect(area.adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH), scale);
            QList<QVector2D> verts;
            verts.reserve(4 * 6 * 2);
            // top frame
            verts.push_back(QVector2D(frame.right(), frame.top()));
            verts.push_back(QVector2D(frame.left(), frame.top()));
            verts.push_back(QVector2D(frame.left(), areaF.top()));
            verts.push_back(QVector2D(frame.left(), areaF.top()));
            verts.push_back(QVector2D(frame.right(), areaF.top()));
            verts.push_back(QVector2D(frame.right(), frame.top()));
            // left frame
            verts.push_back(QVector2D(areaF.left(), frame.top()));
            verts.push_back(QVector2D(frame.left(), frame.top()));
            verts.push_back(QVector2D(frame.left(), frame.bottom()));
            verts.push_back(QVector2D(frame.left(), frame.bottom()));
            verts.push_back(QVector2D(areaF.left(), frame.bottom()));
            verts.push_back(QVector2D(areaF.left(), frame.top()));
            // right frame
            verts.push_back(QVector2D(frame.right(), frame.top()));
            verts.push_back(QVector2D(areaF.right(), frame.top()));
            verts.push_back(QVector2D(areaF.right(), frame.bottom()));
            verts.push_back(QVector2D(areaF.right(), frame.bottom()));
            verts.push_back(QVector2D(frame.right(), frame.bottom()));
            verts.push_back(QVector2D(frame.right(), frame.top()));
            // bottom frame
            verts.push_back(QVector2D(frame.right(), areaF.bottom()));
            verts.push_back(QVector2D(frame.left(), areaF.bottom()));
            verts.push_back(QVector2D(frame.left(), frame.bottom()));
            verts.push_back(QVector2D(frame.left(), frame.bottom()));
            verts.push_back(QVector2D(frame.right(), frame.bottom()));
            verts.push_back(QVector2D(frame.right(), areaF.bottom()));
            vbo->setVertices(verts);

            ShaderBinder binder(ShaderTrait::UniformColor);
            binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
            binder.shader()->setUniform(GLShader::ColorUniform::Color, QColor(0, 0, 0));
            vbo->render(GL_TRIANGLES);
        }
    }
}

void MagnifierEffect::postPaintScreen()
{
    if (m_zoom != m_targetZoom) {
        effects->addRepaint(visibleArea());
    }
    effects->postPaintScreen();
}

QRect MagnifierEffect::magnifierArea(QPointF pos) const
{
    return QRect(pos.x() - m_magnifierSize.width() / 2, pos.y() - m_magnifierSize.height() / 2,
                 m_magnifierSize.width(), m_magnifierSize.height());
}

QRect MagnifierEffect::visibleArea(QPointF pos) const
{
    return magnifierArea(pos).adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH);
}

void MagnifierEffect::zoomIn()
{
    setTargetZoom(m_targetZoom * m_zoomFactor);
}

void MagnifierEffect::zoomOut()
{
    setTargetZoom(m_targetZoom / m_zoomFactor);
}

void MagnifierEffect::toggle()
{
    if (m_zoom == 1.0) {
        if (m_targetZoom == 1.0) {
            setTargetZoom(2.0);
        }
    } else {
        setTargetZoom(1.0);
    }
}

void MagnifierEffect::slotMouseChanged(const QPointF &pos, const QPointF &old,
                                       Qt::MouseButtons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (pos != old && m_zoom != 1) {
        // need full repaint as we might lose some change events on fast mouse movements
        // see Bug 187658
        effects->addRepaintFull();
    }
}

void MagnifierEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowDamaged, this, &MagnifierEffect::slotWindowDamaged);
}

void MagnifierEffect::slotWindowDamaged()
{
    if (isActive()) {
        effects->addRepaint(magnifierArea());
    }
}

bool MagnifierEffect::isActive() const
{
    return m_zoom != 1.0 || m_zoom != m_targetZoom;
}

QSize MagnifierEffect::magnifierSize() const
{
    return m_magnifierSize;
}

qreal MagnifierEffect::targetZoom() const
{
    return m_targetZoom;
}

void MagnifierEffect::saveInitialZoom()
{
    MagnifierConfig::setInitialZoom(m_targetZoom);
    MagnifierConfig::self()->save();
}

void MagnifierEffect::setTargetZoom(double zoomFactor)
{
    const double effectiveTargetZoom = std::clamp(zoomFactor, 1.0, 100.0);
    if (m_targetZoom == effectiveTargetZoom) {
        return;
    }

    m_targetZoom = effectiveTargetZoom;
    effects->addRepaint(visibleArea());
    m_configurationTimer->start();
}

void MagnifierEffect::realtimeZoom(double delta)
{
    // for the change speed to feel roughly linear,
    // we have to increase the delta at higher zoom levels
    delta *= m_targetZoom / 2;
    setTargetZoom(m_targetZoom + delta);
    // skip the animation, we want this to be real time
    m_zoom = m_targetZoom;
}

} // namespace

#include "moc_magnifier.cpp"
