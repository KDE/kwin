/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blur.h"
#include "blurshader.h"
// KConfigSkeleton
#include "blurconfig.h"

#include "libkwineffects/kwinglplatform.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"
#include "utils/xcbutils.h"
#include "wayland/blur_interface.h"
#include "wayland/display.h"
#include "wayland/surface_interface.h"

#include <QGuiApplication>
#include <QMatrix4x4>
#include <QScreen>
#include <QTime>
#include <QTimer>
#include <QWindow>
#include <cmath> // for ceil()
#include <cstdlib>

#include <KConfigGroup>
#include <KSharedConfig>

#include <KDecoration2/Decoration>

namespace KWin
{

static const QByteArray s_blurAtomName = QByteArrayLiteral("_KDE_NET_WM_BLUR_BEHIND_REGION");

KWaylandServer::BlurManagerInterface *BlurEffect::s_blurManager = nullptr;
QTimer *BlurEffect::s_blurManagerRemoveTimer = nullptr;

BlurEffect::BlurEffect()
{
    initConfig<BlurConfig>();
    m_shader = new BlurShader(this);

    initBlurStrengthValues();
    reconfigure(ReconfigureAll);

    if (effects->waylandDisplay()) {
        connect(effects, &EffectsHandler::screenAdded, this, &BlurEffect::screenAdded);
        connect(effects, &EffectsHandler::screenRemoved, this, &BlurEffect::screenRemoved);
        for (const auto screen : effects->screens()) {
            screenAdded(screen);
        }
    } else {
        connect(effects, &EffectsHandler::virtualScreenGeometryChanged, this, [this]() {
            screenGeometryChanged(nullptr);
        });
    }

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (m_shader && m_shader->isValid()) {
        if (effects->xcbConnection()) {
            net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
        }
        if (effects->waylandDisplay()) {
            if (!s_blurManagerRemoveTimer) {
                s_blurManagerRemoveTimer = new QTimer(QCoreApplication::instance());
                s_blurManagerRemoveTimer->setSingleShot(true);
                s_blurManagerRemoveTimer->callOnTimeout([]() {
                    s_blurManager->remove();
                    s_blurManager = nullptr;
                });
            }
            s_blurManagerRemoveTimer->stop();
            if (!s_blurManager) {
                s_blurManager = new KWaylandServer::BlurManagerInterface(effects->waylandDisplay(), s_blurManagerRemoveTimer);
            }
        }
    }

    connect(effects, &EffectsHandler::windowAdded, this, &BlurEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &BlurEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowDecorationChanged, this, &BlurEffect::setupDecorationConnections);
    connect(effects, &EffectsHandler::propertyNotify, this, &BlurEffect::slotPropertyNotify);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this, [this]() {
        if (m_shader && m_shader->isValid()) {
            net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
        }
    });

    // Fetch the blur regions for all windows
    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        slotWindowAdded(window);
    }
}

BlurEffect::~BlurEffect()
{
    // When compositing is restarted, avoid removing the manager immediately.
    if (s_blurManager) {
        s_blurManagerRemoveTimer->start(1000);
    }
}

void BlurEffect::screenAdded(EffectScreen *screen)
{
    connect(screen, &EffectScreen::geometryChanged, this, [this, screen]() {
        screenGeometryChanged(screen);
    });
}

void BlurEffect::screenRemoved(EffectScreen *screen)
{
    disconnect(screen, &EffectScreen::geometryChanged, this, nullptr);
    effects->makeOpenGLContextCurrent();
    m_screenData.erase(screen);
}

void BlurEffect::screenGeometryChanged(EffectScreen *screen)
{
    // Fetch the blur regions for all windows
    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        updateBlurRegion(window);
    }
}

bool BlurEffect::updateTexture(EffectScreen *screen, const RenderTarget &renderTarget)
{
    GLenum textureFormat = GL_RGBA8;
    if (renderTarget.colorDescription() == ColorDescription::sRGB) {
        if (!GLPlatform::instance()->isGLES()) {
            GLuint prevFbo = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prevFbo));

            if (prevFbo != 0) {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            }

            GLenum colorEncoding = GL_LINEAR;
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                                  GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                                  reinterpret_cast<GLint *>(&colorEncoding));

            if (prevFbo != 0) {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevFbo);
            }

            if (colorEncoding == GL_SRGB) {
                textureFormat = GL_SRGB8_ALPHA8;
            }
        }
    } else {
        textureFormat = GL_RGBA16F;
    }

    // Note that we currently render the entire blur effect in logical
    // coordinates - this means that when using high DPI screens the underlying
    // texture will be low DPI. This isn't really visible since we're blurring
    // anyway.
    const auto screenSize = screen ? screen->geometry().size() : effects->virtualScreenSize();

    if (auto it = m_screenData.find(screen); it != m_screenData.end()) {
        const auto &texture = it->second.renderTargetTextures.front();
        if (texture->internalFormat() == textureFormat && texture->size() == screenSize) {
            return true;
        }
    }
    ScreenData data;
    /* Reserve memory for:
     *  - The original sized texture (1)
     *  - The downsized textures (m_downSampleIterations)
     *  - The helper texture (1)
     */
    data.renderTargets.reserve(m_downSampleIterations + 2);
    data.renderTargetTextures.reserve(m_downSampleIterations + 2);

    for (int i = 0; i <= m_downSampleIterations; i++) {
        data.renderTargetTextures.push_back(GLTexture::allocate(textureFormat, screenSize / (1 << i)));
        data.renderTargetTextures.back()->setFilter(GL_LINEAR);
        data.renderTargetTextures.back()->setWrapMode(GL_CLAMP_TO_EDGE);

        data.renderTargets.push_back(std::make_unique<GLFramebuffer>(data.renderTargetTextures.back().get()));
    }

    // This last set is used as a temporary helper texture
    data.renderTargetTextures.push_back(GLTexture::allocate(textureFormat, screenSize));
    data.renderTargetTextures.back()->setFilter(GL_LINEAR);
    data.renderTargetTextures.back()->setWrapMode(GL_CLAMP_TO_EDGE);

    data.renderTargets.push_back(std::make_unique<GLFramebuffer>(data.renderTargetTextures.back().get()));

    const bool renderTargetsValid = std::all_of(data.renderTargets.begin(), data.renderTargets.end(), [](const auto &fbo) {
        return fbo->valid();
    });
    if (!renderTargetsValid) {
        return false;
    }

    // Prepare the stack for the rendering
    data.renderTargetStack.reserve(m_downSampleIterations * 2);

    // Upsample
    for (int i = 1; i < m_downSampleIterations; i++) {
        data.renderTargetStack.push(data.renderTargets[i].get());
    }

    // Downsample
    for (int i = m_downSampleIterations; i > 0; i--) {
        data.renderTargetStack.push(data.renderTargets[i].get());
    }

    // Copysample
    data.renderTargetStack.push(data.renderTargets.front().get());

    m_screenData[screen] = std::move(data);
    return true;
}

void BlurEffect::initBlurStrengthValues()
{
    // This function creates an array of blur strength values that are evenly distributed

    // The range of the slider on the blur settings UI
    int numOfBlurSteps = 15;
    int remainingSteps = numOfBlurSteps;

    /*
     * Explanation for these numbers:
     *
     * The texture blur amount depends on the downsampling iterations and the offset value.
     * By changing the offset we can alter the blur amount without relying on further downsampling.
     * But there is a minimum and maximum value of offset per downsample iteration before we
     * get artifacts.
     *
     * The minOffset variable is the minimum offset value for an iteration before we
     * get blocky artifacts because of the downsampling.
     *
     * The maxOffset value is the maximum offset value for an iteration before we
     * get diagonal line artifacts because of the nature of the dual kawase blur algorithm.
     *
     * The expandSize value is the minimum value for an iteration before we reach the end
     * of a texture in the shader and sample outside of the area that was copied into the
     * texture from the screen.
     */

    // {minOffset, maxOffset, expandSize}
    blurOffsets.append({1.0, 2.0, 10}); // Down sample size / 2
    blurOffsets.append({2.0, 3.0, 20}); // Down sample size / 4
    blurOffsets.append({2.0, 5.0, 50}); // Down sample size / 8
    blurOffsets.append({3.0, 8.0, 150}); // Down sample size / 16
    // blurOffsets.append({5.0, 10.0, 400}); // Down sample size / 32
    // blurOffsets.append({7.0, ?.0});       // Down sample size / 64

    float offsetSum = 0;

    for (int i = 0; i < blurOffsets.size(); i++) {
        offsetSum += blurOffsets[i].maxOffset - blurOffsets[i].minOffset;
    }

    for (int i = 0; i < blurOffsets.size(); i++) {
        int iterationNumber = std::ceil((blurOffsets[i].maxOffset - blurOffsets[i].minOffset) / offsetSum * numOfBlurSteps);
        remainingSteps -= iterationNumber;

        if (remainingSteps < 0) {
            iterationNumber += remainingSteps;
        }

        float offsetDifference = blurOffsets[i].maxOffset - blurOffsets[i].minOffset;

        for (int j = 1; j <= iterationNumber; j++) {
            // {iteration, offset}
            blurStrengthValues.append({i + 1, blurOffsets[i].minOffset + (offsetDifference / iterationNumber) * j});
        }
    }
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    BlurConfig::self()->read();

    int blurStrength = BlurConfig::blurStrength() - 1;
    m_downSampleIterations = blurStrengthValues[blurStrength].iteration;
    m_offset = blurStrengthValues[blurStrength].offset;
    m_expandSize = blurOffsets[m_downSampleIterations - 1].expandSize;
    m_noiseStrength = BlurConfig::noiseStrength();

    m_scalingFactor = std::max(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
    // Invalidate noise texture
    m_noiseTexture.reset();

    // Update all windows for the blur to take effect
    effects->addRepaintFull();
}

void BlurEffect::updateBlurRegion(EffectWindow *w)
{
    QRegion region;
    bool valid = false;

    if (net_wm_blur_region != XCB_ATOM_NONE) {
        const QByteArray value = w->readProperty(net_wm_blur_region, XCB_ATOM_CARDINAL, 32);
        if (value.size() > 0 && !(value.size() % (4 * sizeof(uint32_t)))) {
            const uint32_t *cardinals = reinterpret_cast<const uint32_t *>(value.constData());
            for (unsigned int i = 0; i < value.size() / sizeof(uint32_t);) {
                int x = cardinals[i++];
                int y = cardinals[i++];
                int w = cardinals[i++];
                int h = cardinals[i++];
                region += Xcb::fromXNative(QRect(x, y, w, h)).toRect();
            }
        }
        valid = !value.isNull();
    }

    KWaylandServer::SurfaceInterface *surf = w->surface();

    if (surf && surf->blur()) {
        region = surf->blur()->region();
        valid = true;
    }

    if (auto internal = w->internalWindow()) {
        const auto property = internal->property("kwin_blur");
        if (property.isValid()) {
            region = property.value<QRegion>();
            valid = true;
        }
    }

    if (valid) {
        m_windows[w].region = region;
    } else {
        m_windows.remove(w);
    }
}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    KWaylandServer::SurfaceInterface *surf = w->surface();

    if (surf) {
        windowBlurChangedConnections[w] = connect(surf, &KWaylandServer::SurfaceInterface::blurChanged, this, [this, w]() {
            if (w) {
                updateBlurRegion(w);
            }
        });
    }
    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }

    setupDecorationConnections(w);
    updateBlurRegion(w);
}

void BlurEffect::slotWindowDeleted(EffectWindow *w)
{
    m_windows.remove(w);
    auto it = windowBlurChangedConnections.find(w);
    if (it == windowBlurChangedConnections.end()) {
        return;
    }
    disconnect(*it);
    windowBlurChangedConnections.erase(it);
}

void BlurEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region && net_wm_blur_region != XCB_ATOM_NONE) {
        updateBlurRegion(w);
    }
}

void BlurEffect::setupDecorationConnections(EffectWindow *w)
{
    if (!w->decoration()) {
        return;
    }

    connect(w->decoration(), &KDecoration2::Decoration::blurRegionChanged, this, [this, w]() {
        updateBlurRegion(w);
    });
}

bool BlurEffect::eventFilter(QObject *watched, QEvent *event)
{
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_blur") {
            if (auto w = effects->findWindow(internal)) {
                updateBlurRegion(w);
            }
        }
    }
    return false;
}

bool BlurEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel() && gl->chipClass() < SandyBridge) {
        return false;
    }
    if (gl->isPanfrost() && gl->chipClass() <= MaliT8XX) {
        return false;
    }
    // The blur effect works, but is painfully slow (FPS < 5) on Mali and VideoCore
    if (gl->isLima() || gl->isVideoCore4() || gl->isVideoCore3D()) {
        return false;
    }
    if (gl->isSoftwareEmulation()) {
        return false;
    }

    return true;
}

bool BlurEffect::supported()
{
    bool supported = effects->isOpenGLCompositing() && GLFramebuffer::supported() && GLFramebuffer::blitSupported();

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

bool BlurEffect::decorationSupportsBlurBehind(const EffectWindow *w) const
{
    return w->decoration() && !w->decoration()->blurRegion().isNull();
}

QRegion BlurEffect::decorationBlurRegion(const EffectWindow *w) const
{
    if (!decorationSupportsBlurBehind(w)) {
        return QRegion();
    }

    QRegion decorationRegion = QRegion(w->decoration()->rect()) - w->decorationInnerRect().toRect();
    //! we return only blurred regions that belong to decoration region
    return decorationRegion.intersected(w->decoration()->blurRegion());
}

QRect BlurEffect::expand(const QRect &rect) const
{
    return rect.adjusted(-m_expandSize, -m_expandSize, m_expandSize, m_expandSize);
}

QRegion BlurEffect::expand(const QRegion &region) const
{
    QRegion expanded;

    for (const QRect &rect : region) {
        expanded += expand(rect);
    }

    return expanded;
}

QRegion BlurEffect::blurRegion(EffectWindow *w) const
{
    QRegion region;

    if (auto it = m_windows.find(w); it != m_windows.end()) {
        const QRegion &appRegion = it->region;
        if (!appRegion.isEmpty()) {
            if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
                region = decorationBlurRegion(w);
            }
            region |= appRegion.translated(w->contentsRect().topLeft().toPoint()) & w->decorationInnerRect().toRect();
        } else {
            // An empty region means that the blur effect should be enabled
            // for the whole window.
            region = w->rect().toRect();
        }
    } else if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
        // If the client hasn't specified a blur region, we'll only enable
        // the effect behind the decoration.
        region = decorationBlurRegion(w);
    }

    return region;
}

void BlurEffect::uploadRegion(const std::span<QVector2D> map, size_t &index, const QRegion &region)
{
    for (const QRect &r : region) {
        const QVector2D topLeft(r.x(), r.y());
        const QVector2D topRight((r.x() + r.width()), r.y());
        const QVector2D bottomLeft(r.x(), (r.y() + r.height()));
        const QVector2D bottomRight((r.x() + r.width()), (r.y() + r.height()));

        // First triangle
        map[index++] = topRight;
        map[index++] = topLeft;
        map[index++] = bottomLeft;

        // Second triangle
        map[index++] = bottomLeft;
        map[index++] = bottomRight;
        map[index++] = topRight;
    }
}

bool BlurEffect::uploadGeometry(GLVertexBuffer *vbo, const QRegion &expandedBlurRegion, const QRegion &blurRegion)
{
    const int vertexCount = (expandedBlurRegion.rectCount() + blurRegion.rectCount()) * 6;
    if (!vertexCount) {
        return false;
    }

    auto map = vbo->map<QVector2D>(vertexCount);
    if (!map) {
        return false;
    }

    size_t index = 0;
    uploadRegion(*map, index, expandedBlurRegion);
    uploadRegion(*map, index, blurRegion);

    vbo->unmap();

    const GLVertexAttrib layout[] = {
        {VA_Position, 2, GL_FLOAT, 0},
        {VA_TexCoord, 2, GL_FLOAT, 0}};

    vbo->setAttribLayout(layout, 2, sizeof(QVector2D));
    return true;
}

void BlurEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    m_paintedArea = QRegion();
    m_currentBlur = QRegion();
    m_currentScreen = effects->waylandDisplay() ? data.screen : nullptr;

    effects->prePaintScreen(data, presentTime);
}

void BlurEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // this effect relies on prePaintWindow being called in the bottom to top order

    effects->prePaintWindow(w, data, presentTime);

    if (!m_shader || !m_shader->isValid()) {
        return;
    }

    const QRegion oldOpaque = data.opaque;
    if (data.opaque.intersects(m_currentBlur)) {
        // to blur an area partially we have to shrink the opaque area of a window
        QRegion newOpaque;
        for (const QRect &rect : data.opaque) {
            newOpaque |= rect.adjusted(m_expandSize, m_expandSize, -m_expandSize, -m_expandSize);
        }
        data.opaque = newOpaque;

        // we don't have to blur a region we don't see
        m_currentBlur -= newOpaque;
    }

    // if we have to paint a non-opaque part of this window that intersects with the
    // currently blurred region we have to redraw the whole region
    if ((data.paint - oldOpaque).intersects(m_currentBlur)) {
        data.paint |= m_currentBlur;
    }

    // in case this window has regions to be blurred
    const QRect screen = effects->virtualScreenGeometry();
    const QRegion blurArea = blurRegion(w).translated(w->pos().toPoint()) & screen;

    // if this window or a window underneath the blurred area is painted again we have to
    // blur everything
    if (m_paintedArea.intersects(blurArea) || data.paint.intersects(blurArea)) {
        data.paint |= blurArea;
        // we have to check again whether we do not damage a blurred area
        // of a window
        if (blurArea.intersects(m_currentBlur)) {
            data.paint |= m_currentBlur;
        }
    }

    m_currentBlur |= blurArea;

    m_paintedArea -= data.opaque;
    m_paintedArea |= data.paint;
}

bool BlurEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!m_shader || !m_shader->isValid()) {
        return false;
    }

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }

    if (w->isDesktop()) {
        return false;
    }

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if ((scaled || (translated || (mask & PAINT_WINDOW_TRANSFORMED))) && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }

    return true;
}

void BlurEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (shouldBlur(w, mask, data)) {
        if (!updateTexture(m_currentScreen, renderTarget)) {
            return;
        }
        const QRect screen = viewport.renderRect().toRect();
        QRegion shape = blurRegion(w).translated(w->pos().toPoint());

        // let's do the evil parts - someone wants to blur behind a transformed window
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
            shape = scaledShape;

            // Only translated, not scaled
        } else if (translated) {
            QRegion translated;
            for (QRect r : shape) {
                const QRectF t = QRectF(r).translated(data.xTranslation(), data.yTranslation());
                const QPoint topLeft(std::ceil(t.x()), std::ceil(t.y()));
                const QPoint bottomRight(std::floor(t.x() + t.width() - 1), std::floor(t.y() + t.height() - 1));
                translated |= QRect(topLeft, bottomRight);
            }
            shape = translated;
        }

        shape &= region;

        if (!shape.isEmpty()) {
            doBlur(renderTarget, viewport, shape, screen, data.opacity(), w->frameGeometry().toRect());
        }
    }

    // Draw the window over the blurred area
    effects->drawWindow(renderTarget, viewport, w, mask, region, data);
}

void BlurEffect::generateNoiseTexture()
{
    if (m_noiseStrength == 0) {
        return;
    }

    // Init randomness based on time
    std::srand((uint)QTime::currentTime().msec());

    QImage noiseImage(QSize(256, 256), QImage::Format_Grayscale8);

    for (int y = 0; y < noiseImage.height(); y++) {
        uint8_t *noiseImageLine = (uint8_t *)noiseImage.scanLine(y);

        for (int x = 0; x < noiseImage.width(); x++) {
            noiseImageLine[x] = std::rand() % m_noiseStrength;
        }
    }

    // The noise texture looks distorted when not scaled with integer
    noiseImage = noiseImage.scaled(noiseImage.size() * m_scalingFactor);

    m_noiseTexture = GLTexture::upload(noiseImage);
    if (!m_noiseTexture) {
        return;
    }
    m_noiseTexture->setFilter(GL_NEAREST);
    m_noiseTexture->setWrapMode(GL_REPEAT);
}

void BlurEffect::doBlur(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &shape, const QRect &screen, const float opacity, QRect windowRect)
{
    const auto &outputData = m_screenData[m_currentScreen];
    const QRegion expandedBlurRegion = expand(shape) & expand(screen);

    const bool useSRGB = outputData.renderTargetTextures.front()->internalFormat() == GL_SRGB8_ALPHA8;

    // Upload geometry for the down and upsample iterations
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();

    if (!uploadGeometry(vbo, expandedBlurRegion, shape)) {
        return;
    }

    const QRect logicalSourceRect = expandedBlurRegion.boundingRect() & screen;
    const int blurRectCount = expandedBlurRegion.rectCount() * 6;

    QMatrix4x4 projection;
    projection.ortho(viewport.renderRect().x(), viewport.renderRect().x() + viewport.renderRect().width(),
                     viewport.renderRect().y() + viewport.renderRect().height(), viewport.renderRect().y(), 0, 65535);

    // This assumes the source frame buffer is in device coordinates, while
    // our target framebuffer is in logical coordinates. It's a bit ugly but
    // to fix it properly we probably need to do blits in normalized
    // coordinates.
    outputData.renderTargets.back()->blitFromRenderTarget(renderTarget, viewport, logicalSourceRect, logicalSourceRect.translated(-screen.topLeft()));
    GLFramebuffer::pushFramebuffers(outputData.renderTargetStack);

    if (useSRGB) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }

    vbo->bindArrays();
    copyScreenSampleTexture(outputData, viewport, outputData.renderTargetStack.top()->size(), vbo, blurRectCount, shape.boundingRect().translated(-screen.topLeft()), projection);
    downSampleTexture(outputData, vbo, blurRectCount, projection);
    upSampleTexture(outputData, vbo, blurRectCount, projection);

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
        glEnable(GL_BLEND);
#if 1 // bow shape, always above y = x
        float o = 1.0f - opacity;
        o = 1.0f - o * o;
#else // sigmoid shape, above y = x for x > 0.5, below y = x for x < 0.5
        float o = 2.0f * opacity - 1.0f;
        o = 0.5f + o / (1.0f + std::abs(o));
#endif
        glBlendColor(0, 0, 0, o);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    upscaleRenderToScreen(outputData, renderTarget, viewport, vbo, blurRectCount, shape.rectCount() * 6, windowRect.topLeft());

    if (useSRGB) {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    if (m_noiseStrength > 0) {
        // Apply an additive noise onto the blurred image.
        // The noise is useful to mask banding artifacts, which often happens due to the smooth color transitions in the
        // blurred image.
        // The noise is applied in perceptual space (i.e. after glDisable(GL_FRAMEBUFFER_SRGB)). This practice is also
        // seen in other application of noise synthesis (films, image codecs), and makes the noise less visible overall
        // (reduces graininess).
        glEnable(GL_BLEND);
        if (opacity < 1.0) {
            // We need to modulate the opacity of the noise as well; otherwise a thin layer would appear when applying
            // effects like fade out.
            // glBlendColor should have been set above.
            glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
        } else {
            // Add the shader's output directly to the pixels in framebuffer.
            glBlendFunc(GL_ONE, GL_ONE);
        }
        applyNoise(outputData, renderTarget, viewport, vbo, blurRectCount, shape.rectCount() * 6, windowRect.topLeft());
        glDisable(GL_BLEND);
    }

    vbo->unbindArrays();
}

void BlurEffect::upscaleRenderToScreen(const ScreenData &data, const RenderTarget &renderTarget, const RenderViewport &viewport, GLVertexBuffer *vbo, int vboStart, int blurRectCount, QPoint windowPosition)
{
    const auto &tex = data.renderTargetTextures[1];
    tex->bind();

    m_shader->bind(BlurShader::UpSampleType);
    QMatrix4x4 fragCoordToUv;
    fragCoordToUv.translate(0.5, 0.5);
    fragCoordToUv *= renderTarget.transformation().inverted();
    fragCoordToUv.translate(-0.5, -0.5);
    fragCoordToUv.scale(1.0 / renderTarget.size().width(), 1.0 / renderTarget.size().height());
    m_shader->setFragCoordToUv(fragCoordToUv);
    m_shader->setTargetTextureSize(tex->size() * viewport.scale());

    m_shader->setOffset(m_offset);
    QMatrix4x4 projection = viewport.projectionMatrix();
    // the vbo is in logical coordinates, adjust for that
    projection.scale(viewport.scale(), viewport.scale());
    m_shader->setModelViewProjectionMatrix(projection);

    // Render to the screen
    vbo->draw(GL_TRIANGLES, vboStart, blurRectCount);
    m_shader->unbind();
}

void BlurEffect::applyNoise(const ScreenData &data, const RenderTarget &renderTarget, const RenderViewport &viewport, GLVertexBuffer *vbo, int vboStart, int blurRectCount, QPoint windowPosition)
{
    if (!m_noiseTexture) {
        generateNoiseTexture();
        if (!m_noiseTexture) {
            return;
        }
    }

    m_shader->bind(BlurShader::NoiseSampleType);
    m_shader->setTargetTextureSize(renderTarget.size());
    m_shader->setNoiseTextureSize(m_noiseTexture->size());
    m_shader->setTexturePosition(windowPosition);

    m_noiseTexture->bind();

    m_shader->setOffset(m_offset);
    QMatrix4x4 projection = viewport.projectionMatrix();
    // the vbo is in logical coordinates, adjust for that
    projection.scale(viewport.scale(), viewport.scale());
    m_shader->setModelViewProjectionMatrix(projection);

    vbo->draw(GL_TRIANGLES, vboStart, blurRectCount);
    m_shader->unbind();
}

void BlurEffect::downSampleTexture(const ScreenData &data, GLVertexBuffer *vbo, int blurRectCount, const QMatrix4x4 &projection)
{
    m_shader->bind(BlurShader::DownSampleType);
    m_shader->setOffset(m_offset);
    m_shader->setModelViewProjectionMatrix(projection);

    for (int i = 1; i <= m_downSampleIterations; i++) {
        m_shader->setTargetTextureSize(data.renderTargetTextures[i]->size());

        // Copy the image from this texture
        data.renderTargetTextures[i - 1]->bind();

        vbo->draw(GL_TRIANGLES, 0, blurRectCount);
        GLFramebuffer::popFramebuffer();
    }

    m_shader->unbind();
}

void BlurEffect::upSampleTexture(const ScreenData &data, GLVertexBuffer *vbo, int blurRectCount, const QMatrix4x4 &projection)
{
    m_shader->bind(BlurShader::UpSampleType);
    m_shader->setOffset(m_offset);
    m_shader->setModelViewProjectionMatrix(projection);

    for (int i = m_downSampleIterations - 1; i >= 1; i--) {
        const auto &tex = data.renderTargetTextures[i];

        QMatrix4x4 fragCoordToUv;
        fragCoordToUv.scale(1.0 / tex->width(), 1.0 / tex->height());
        m_shader->setFragCoordToUv(fragCoordToUv);
        m_shader->setTargetTextureSize(tex->size());

        // Copy the image from this texture
        data.renderTargetTextures[i + 1]->bind();

        vbo->draw(GL_TRIANGLES, 0, blurRectCount);
        GLFramebuffer::popFramebuffer();
    }

    m_shader->unbind();
}

void BlurEffect::copyScreenSampleTexture(const ScreenData &data, const RenderViewport &viewport, const QSize &fboSize, GLVertexBuffer *vbo, int blurRectCount, const QRect &boundingRect, const QMatrix4x4 &projection)
{
    m_shader->bind(BlurShader::CopySampleType);

    m_shader->setModelViewProjectionMatrix(projection);
    m_shader->setTargetTextureSize(fboSize);

    /*
     * This '1' sized adjustment is necessary do avoid windows affecting the blur that are
     * right next to this window.
     */
    m_shader->setBlurRect(boundingRect.adjusted(1, 1, -1, -1), viewport.renderRect().size());
    data.renderTargetTextures.back()->bind();

    vbo->draw(GL_TRIANGLES, 0, blurRectCount);
    GLFramebuffer::popFramebuffer();

    m_shader->unbind();
}

bool BlurEffect::isActive() const
{
    return !effects->isScreenLocked();
}

bool BlurEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_blur.cpp"
