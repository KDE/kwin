/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "plugins/shakecursor/shakecursorconfig.h"

namespace KWin
{

ShakeCursorEffect::ShakeCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    m_resetCursorScaleTimer.setSingleShot(true);
    connect(&m_resetCursorScaleTimer, &QTimer::timeout, this, [this]() {
        m_resetCursorScaleAnimation.setStartValue(m_cursorMagnification);
        m_resetCursorScaleAnimation.setEndValue(1.0);
        m_resetCursorScaleAnimation.setDuration(animationTime(150));
        m_resetCursorScaleAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        m_resetCursorScaleAnimation.start();
    });

    connect(&m_resetCursorScaleAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        update(Transaction{
            .position = m_cursor->pos(),
            .hotspot = m_cursor->hotspot(),
            .size = m_cursor->geometry().size(),
            .magnification = m_resetCursorScaleAnimation.currentValue().toReal(),
        });
    });

    ShakeCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

ShakeCursorEffect::~ShakeCursorEffect()
{
    showCursor();
}

bool ShakeCursorEffect::supported()
{
    if (!effects->waylandDisplay()) {
        return false;
    }
    return effects->isOpenGLCompositing();
}

void ShakeCursorEffect::reconfigure(ReconfigureFlags flags)
{
    ShakeCursorConfig::self()->read();

    m_shakeDetector.setInterval(ShakeCursorConfig::timeInterval());
    m_shakeDetector.setSensitivity(ShakeCursorConfig::sensitivity());
}

bool ShakeCursorEffect::isActive() const
{
    return m_cursorMagnification != 1.0;
}

void ShakeCursorEffect::pointerEvent(MouseEvent *event)
{
    if (event->type() != QEvent::MouseMove) {
        return;
    }

    if (const auto shakeFactor = m_shakeDetector.update(event)) {
        update(Transaction{
            .position = m_cursor->pos(),
            .hotspot = m_cursor->hotspot(),
            .size = m_cursor->geometry().size(),
            .magnification = std::max(m_cursorMagnification, 1.0 + ShakeCursorConfig::magnification() * shakeFactor.value()),
        });
        m_resetCursorScaleTimer.start(1000);
        m_resetCursorScaleAnimation.stop();
    } else {
        update(Transaction{
            .magnification = 1.0,
        });
        m_resetCursorScaleTimer.stop();
        m_resetCursorScaleAnimation.stop();
    }
}

GLTexture *ShakeCursorEffect::ensureCursorTexture()
{
    if (!m_cursorTexture || m_cursorTextureDirty) {
        m_cursorTexture.reset();
        m_cursorTextureDirty = false;
        const auto cursor = effects->cursorImage();
        if (!cursor.image().isNull()) {
            m_cursorTexture = GLTexture::upload(cursor.image());
            if (!m_cursorTexture) {
                return nullptr;
            }
            m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
            m_cursorTexture->setFilter(GL_LINEAR);
        }
    }
    return m_cursorTexture.get();
}

void ShakeCursorEffect::markCursorTextureDirty()
{
    m_cursorTextureDirty = true;

    update(Transaction{
        .position = m_cursor->pos(),
        .hotspot = m_cursor->hotspot(),
        .size = m_cursor->geometry().size(),
        .magnification = m_cursorMagnification,
        .damaged = true,
    });
}

void ShakeCursorEffect::showCursor()
{
    if (m_mouseHidden) {
        disconnect(effects, &EffectsHandler::cursorShapeChanged, this, &ShakeCursorEffect::markCursorTextureDirty);
        effects->showCursor();
        if (m_cursorTexture) {
            effects->makeOpenGLContextCurrent();
            m_cursorTexture.reset();
        }
        m_cursorTextureDirty = false;
        m_mouseHidden = false;
    }
}

void ShakeCursorEffect::hideCursor()
{
    if (!m_mouseHidden) {
        effects->hideCursor();
        connect(effects, &EffectsHandler::cursorShapeChanged, this, &ShakeCursorEffect::markCursorTextureDirty);
        m_mouseHidden = true;
    }
}

void ShakeCursorEffect::update(const Transaction &transaction)
{
    if (transaction.magnification == 1.0) {
        if (m_cursorMagnification == 1.0) {
            return;
        }

        const QRectF oldCursorGeometry = m_cursorGeometry;
        showCursor();

        m_cursorGeometry = QRectF();
        m_cursorMagnification = 1.0;

        effects->addRepaint(oldCursorGeometry);
    } else {
        const QRectF oldCursorGeometry = m_cursorGeometry;
        hideCursor();

        m_cursorMagnification = transaction.magnification;
        m_cursorGeometry = QRectF(transaction.position - transaction.hotspot * transaction.magnification, transaction.size * transaction.magnification);

        if (transaction.damaged || oldCursorGeometry != m_cursorGeometry) {
            effects->addRepaint(oldCursorGeometry.united(m_cursorGeometry));
        }
    }
}

void ShakeCursorEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    if (GLTexture *texture = ensureCursorTexture()) {
        const bool clipping = region != infiniteRegion();
        const QRegion clipRegion = clipping ? viewport.mapToRenderTarget(region) : infiniteRegion();
        if (clipping) {
            glEnable(GL_SCISSOR_TEST);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        shader->setColorspaceUniformsFromSRGB(renderTarget.colorDescription());
        QMatrix4x4 mvp = viewport.projectionMatrix();
        mvp.translate(m_cursorGeometry.x() * viewport.scale(), m_cursorGeometry.y() * viewport.scale());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        texture->render(clipRegion, m_cursorGeometry.size() * viewport.scale(), clipping);
        ShaderManager::instance()->popShader();
        glDisable(GL_BLEND);

        if (clipping) {
            glDisable(GL_SCISSOR_TEST);
        }
    }
}

} // namespace KWin

#include "moc_shakecursor.cpp"
