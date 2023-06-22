/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "contrast.h"
#include "contrastshader.h"
// KConfigSkeleton

#include "utils/xcbutils.h"
#include "wayland/contrast_interface.h"
#include "wayland/display.h"
#include "wayland/surface_interface.h"

#include <QCoreApplication>
#include <QMatrix4x4>
#include <QTimer>
#include <QWindow>
#include <cmath> // for ceil()

namespace KWin
{

static const QByteArray s_contrastAtomName = QByteArrayLiteral("_KDE_NET_WM_BACKGROUND_CONTRAST_REGION");

KWaylandServer::ContrastManagerInterface *ContrastEffect::s_contrastManager = nullptr;
QTimer *ContrastEffect::s_contrastManagerRemoveTimer = nullptr;

ContrastEffect::ContrastEffect()
{
    m_shader = std::make_unique<ContrastShader>();
    m_shader->init();

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (m_shader && m_shader->isValid()) {
        if (effects->xcbConnection()) {
            m_net_wm_contrast_region = effects->announceSupportProperty(s_contrastAtomName, this);
        }
        if (effects->waylandDisplay()) {
            if (!s_contrastManagerRemoveTimer) {
                s_contrastManagerRemoveTimer = new QTimer(QCoreApplication::instance());
                s_contrastManagerRemoveTimer->setSingleShot(true);
                s_contrastManagerRemoveTimer->callOnTimeout([]() {
                    s_contrastManager->remove();
                    s_contrastManager = nullptr;
                });
            }
            s_contrastManagerRemoveTimer->stop();
            if (!s_contrastManager) {
                s_contrastManager = new KWaylandServer::ContrastManagerInterface(effects->waylandDisplay(), s_contrastManagerRemoveTimer);
            }
        }
    }

    connect(effects, &EffectsHandler::windowAdded, this, &ContrastEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &ContrastEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::propertyNotify, this, &ContrastEffect::slotPropertyNotify);
    connect(effects, &EffectsHandler::virtualScreenGeometryChanged, this, &ContrastEffect::slotScreenGeometryChanged);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this, [this]() {
        if (m_shader && m_shader->isValid()) {
            m_net_wm_contrast_region = effects->announceSupportProperty(s_contrastAtomName, this);
        }
    });

    // Fetch the contrast regions for all windows
    const EffectWindowList windowList = effects->stackingOrder();
    for (EffectWindow *window : windowList) {
        slotWindowAdded(window);
    }
}

ContrastEffect::~ContrastEffect()
{
    // When compositing is restarted, avoid removing the manager immediately.
    if (s_contrastManager) {
        s_contrastManagerRemoveTimer->start(1000);
    }
}

void ContrastEffect::slotScreenGeometryChanged()
{
    effects->makeOpenGLContextCurrent();
    if (!supported()) {
        effects->reloadEffect(this);
        return;
    }

    const EffectWindowList windowList = effects->stackingOrder();
    for (EffectWindow *window : windowList) {
        updateContrastRegion(window);
    }
}

void ContrastEffect::updateContrastRegion(EffectWindow *w)
{
    QRegion region;
    QMatrix4x4 matrix;
    float colorTransform[16];
    bool valid = false;

    if (m_net_wm_contrast_region != XCB_ATOM_NONE) {
        const QByteArray value = w->readProperty(m_net_wm_contrast_region, m_net_wm_contrast_region, 32);

        if (value.size() > 0 && !((value.size() - (16 * sizeof(uint32_t))) % ((4 * sizeof(uint32_t))))) {
            const uint32_t *cardinals = reinterpret_cast<const uint32_t *>(value.constData());
            const float *floatCardinals = reinterpret_cast<const float *>(value.constData());
            unsigned int i = 0;
            for (; i < ((value.size() - (16 * sizeof(uint32_t)))) / sizeof(uint32_t);) {
                int x = cardinals[i++];
                int y = cardinals[i++];
                int w = cardinals[i++];
                int h = cardinals[i++];
                region += Xcb::fromXNative(QRect(x, y, w, h)).toRect();
            }

            for (unsigned int j = 0; j < 16; ++j) {
                colorTransform[j] = floatCardinals[i + j];
            }

            matrix = QMatrix4x4(colorTransform);
        }

        valid = !value.isNull();
    }

    KWaylandServer::SurfaceInterface *surf = w->surface();

    if (surf && surf->contrast()) {
        region = surf->contrast()->region();
        matrix = colorMatrix(surf->contrast()->contrast(), surf->contrast()->intensity(), surf->contrast()->saturation());
        valid = true;
    }

    if (auto internal = w->internalWindow()) {
        const auto property = internal->property("kwin_background_region");
        if (property.isValid()) {
            region = property.value<QRegion>();
            bool ok = false;
            qreal contrast = internal->property("kwin_background_contrast").toReal(&ok);
            if (!ok) {
                contrast = 1.0;
            }
            qreal intensity = internal->property("kwin_background_intensity").toReal(&ok);
            if (!ok) {
                intensity = 1.0;
            }
            qreal saturation = internal->property("kwin_background_saturation").toReal(&ok);
            if (!ok) {
                saturation = 1.0;
            }
            matrix = colorMatrix(contrast, intensity, saturation);
            valid = true;
        }
    }

    if (valid) {
        m_windowData[w] = {
            .colorMatrix = matrix,
            .contrastRegion = region,
        };
    } else {
        m_windowData.remove(w);
    }
}

void ContrastEffect::slotWindowAdded(EffectWindow *w)
{
    KWaylandServer::SurfaceInterface *surf = w->surface();

    if (surf) {
        m_contrastChangedConnections[w] = connect(surf, &KWaylandServer::SurfaceInterface::contrastChanged, this, [this, w]() {
            if (w) {
                updateContrastRegion(w);
            }
        });
    }

    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }

    updateContrastRegion(w);
}

bool ContrastEffect::eventFilter(QObject *watched, QEvent *event)
{
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_background_region" || pe->propertyName() == "kwin_background_contrast" || pe->propertyName() == "kwin_background_intensity" || pe->propertyName() == "kwin_background_saturation") {
            if (auto w = effects->findWindow(internal)) {
                updateContrastRegion(w);
            }
        }
    }
    return false;
}

void ContrastEffect::slotWindowDeleted(EffectWindow *w)
{
    if (m_contrastChangedConnections.contains(w)) {
        disconnect(m_contrastChangedConnections[w]);
        m_contrastChangedConnections.remove(w);
    }
    m_windowData.remove(w);
}

void ContrastEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == m_net_wm_contrast_region && m_net_wm_contrast_region != XCB_ATOM_NONE) {
        updateContrastRegion(w);
    }
}

QMatrix4x4 ContrastEffect::colorMatrix(qreal contrast, qreal intensity, qreal saturation)
{
    QMatrix4x4 satMatrix; // saturation
    QMatrix4x4 intMatrix; // intensity
    QMatrix4x4 contMatrix; // contrast

    // Saturation matrix
    if (!qFuzzyCompare(saturation, 1.0)) {
        const qreal rval = (1.0 - saturation) * .2126;
        const qreal gval = (1.0 - saturation) * .7152;
        const qreal bval = (1.0 - saturation) * .0722;

        satMatrix = QMatrix4x4(rval + saturation, rval, rval, 0.0,
                               gval, gval + saturation, gval, 0.0,
                               bval, bval, bval + saturation, 0.0,
                               0, 0, 0, 1.0);
    }

    // IntensityMatrix
    if (!qFuzzyCompare(intensity, 1.0)) {
        intMatrix.scale(intensity, intensity, intensity);
    }

    // Contrast Matrix
    if (!qFuzzyCompare(contrast, 1.0)) {
        const float transl = (1.0 - contrast) / 2.0;

        contMatrix = QMatrix4x4(contrast, 0, 0, 0.0,
                                0, contrast, 0, 0.0,
                                0, 0, contrast, 0.0,
                                transl, transl, transl, 1.0);
    }

    QMatrix4x4 colorMatrix = contMatrix * satMatrix * intMatrix;
    // colorMatrix = colorMatrix.transposed();

    return colorMatrix;
}

bool ContrastEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel() && gl->chipClass() < SandyBridge) {
        return false;
    }
    if (gl->isPanfrost() && gl->chipClass() <= MaliT8XX) {
        return false;
    }
    if (gl->isLima() || gl->isVideoCore4() || gl->isVideoCore3D()) {
        return false;
    }
    if (gl->isSoftwareEmulation()) {
        return false;
    }

    return true;
}

bool ContrastEffect::supported()
{
    bool supported = effects->isOpenGLCompositing() && GLFramebuffer::supported();

    if (supported) {
        int maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        const QSize screenSize = effects->virtualScreenSize();
        if (screenSize.width() > maxTexSize || screenSize.height() > maxTexSize) {
            supported = false;
        }
    }
    return supported;
}

QRegion ContrastEffect::contrastRegion(const EffectWindow *w) const
{
    QRegion region;
    if (const auto it = m_windowData.find(w); it != m_windowData.end()) {
        const QRegion &appRegion = it->contrastRegion;
        if (!appRegion.isEmpty()) {
            region |= appRegion.translated(w->contentsRect().topLeft().toPoint()) & w->decorationInnerRect().toRect();
        } else {
            // An empty region means that the blur effect should be enabled
            // for the whole window.
            region = w->decorationInnerRect().toRect();
        }
    }

    return region;
}

void ContrastEffect::uploadRegion(QVector2D *&map, const QRegion &region, qreal scale)
{
    Q_ASSERT(map);
    for (const QRect &r : region) {
        const auto deviceRect = scaledRect(r, scale);
        const QVector2D topLeft = roundVector(QVector2D(deviceRect.x(), deviceRect.y()));
        const QVector2D topRight = roundVector(QVector2D(deviceRect.x() + deviceRect.width(), deviceRect.y()));
        const QVector2D bottomLeft = roundVector(QVector2D(deviceRect.x(), deviceRect.y() + deviceRect.height()));
        const QVector2D bottomRight = roundVector(QVector2D(deviceRect.x() + deviceRect.width(), deviceRect.y() + deviceRect.height()));

        // First triangle
        *(map++) = topRight;
        *(map++) = topLeft;
        *(map++) = bottomLeft;

        // Second triangle
        *(map++) = bottomLeft;
        *(map++) = bottomRight;
        *(map++) = topRight;
    }
}

bool ContrastEffect::uploadGeometry(GLVertexBuffer *vbo, const QRegion &region, qreal scale)
{
    const int vertexCount = region.rectCount() * 6;
    if (!vertexCount) {
        return false;
    }

    QVector2D *map = (QVector2D *)vbo->map(vertexCount * sizeof(QVector2D));
    if (!map) {
        return false;
    }
    uploadRegion(map, region, scale);
    vbo->unmap();

    const GLVertexAttrib layout[] = {
        {VA_Position, 2, GL_FLOAT, 0},
        {VA_TexCoord, 2, GL_FLOAT, 0}};

    vbo->setAttribLayout(layout, 2, sizeof(QVector2D));
    return true;
}

bool ContrastEffect::shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!m_shader || !m_shader->isValid()) {
        return false;
    }

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBackgroundContrastRole).toBool()) {
        return false;
    }

    if (w->isDesktop()) {
        return false;
    }

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if ((scaled || (translated || (mask & PAINT_WINDOW_TRANSFORMED))) && !w->data(WindowForceBackgroundContrastRole).toBool()) {
        return false;
    }

    return true;
}

void ContrastEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (shouldContrast(w, mask, data)) {
        const QRect screen = effects->renderTargetRect();
        QRegion shape = region & contrastRegion(w).translated(w->pos().toPoint()) & screen;

        // let's do the evil parts - someone wants to contrast behind a transformed window
        const bool translated = data.xTranslation() || data.yTranslation();
        const bool scaled = data.xScale() != 1 || data.yScale() != 1;
        if (scaled) {
            QPoint pt = shape.boundingRect().topLeft();
            QRegion scaledShape;
            for (QRect r : shape) {
                const QPointF topLeft(pt.x() + (r.x() - pt.x()) * data.xScale() + data.xTranslation(),
                                      pt.y() + (r.y() - pt.y()) * data.yScale() + data.yTranslation());
                const QPoint bottomRight(std::floor(topLeft.x() + r.width() * data.xScale()) - 1,
                                         std::floor(topLeft.y() + r.height() * data.yScale()) - 1);
                scaledShape |= QRect(QPoint(std::floor(topLeft.x()), std::floor(topLeft.y())), bottomRight);
            }
            shape = scaledShape & region;

            // Only translated, not scaled
        } else if (translated) {
            QRegion translated;
            for (QRect r : shape) {
                const QRectF t = QRectF(r).translated(data.xTranslation(), data.yTranslation());
                const QPoint topLeft(std::ceil(t.x()), std::ceil(t.y()));
                const QPoint bottomRight(std::floor(t.x() + t.width() - 1), std::floor(t.y() + t.height() - 1));
                translated |= QRect(topLeft, bottomRight);
            }
            shape = translated & region;
        }

        if (!shape.isEmpty()) {
            doContrast(w, shape, screen, data.opacity(), data.projectionMatrix());
        }
    }

    // Draw the window over the contrast area
    effects->drawWindow(w, mask, region, data);
}

void ContrastEffect::doContrast(EffectWindow *w, const QRegion &shape, const QRect &screen, const float opacity, const QMatrix4x4 &screenProjection)
{
    const qreal scale = effects->renderTargetScale();
    const QRegion actualShape = shape & screen;
    const QRectF r = scaledRect(actualShape.boundingRect(), scale);

    // Upload geometry for the horizontal and vertical passes
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    if (!uploadGeometry(vbo, actualShape, scale)) {
        return;
    }
    vbo->bindArrays();

    // Create a scratch texture and copy the area in the back buffer that we're
    // going to blur into it
    GLTexture scratch(GL_RGBA8, r.width(), r.height());
    scratch.setFilter(GL_LINEAR);
    scratch.setWrapMode(GL_CLAMP_TO_EDGE);
    scratch.bind();

    const QRectF sg = scaledRect(effects->renderTargetRect(), scale);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (r.x() - sg.x()), (sg.height() - (r.y() - sg.y() + r.height())),
                        scratch.width(), scratch.height());

    // Draw the texture on the offscreen framebuffer object, while blurring it horizontally

    m_shader->setColorMatrix(m_windowData.value(w).colorMatrix);
    m_shader->bind();

    m_shader->setOpacity(opacity);
    // Set up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    QMatrix4x4 textureMatrix;
    textureMatrix.scale(1.0 / r.width(), -1.0 / r.height(), 1);
    textureMatrix.translate(-r.x(), -r.height() - r.y(), 0);
    m_shader->setTextureMatrix(textureMatrix);
    m_shader->setModelViewProjectionMatrix(screenProjection);

    vbo->draw(GL_TRIANGLES, 0, actualShape.rectCount() * 6);

    scratch.unbind();
    scratch.discard();

    vbo->unbindArrays();

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    m_shader->unbind();
}

bool ContrastEffect::isActive() const
{
    return !effects->isScreenLocked();
}

bool ContrastEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin
