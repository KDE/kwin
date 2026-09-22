/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"
#include "plugins/shakecursor/shakecursorconfig.h"
#include "pointer_input.h"
#include "scene/imageitem.h"
#include "scene/workspacescene.h"

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(shakecursor);
}

namespace KWin
{

ShakeCursorItem::ShakeCursorItem(const CursorTheme &theme, Item *parent)
    : Item(parent)
{
    m_source = std::make_unique<ShapeCursorSource>();
    m_source->setTheme(theme);
    m_source->setShape(Qt::ArrowCursor);

    refresh();
    connect(m_source.get(), &CursorSource::changed, this, &ShakeCursorItem::refresh);
}

void ShakeCursorItem::refresh()
{
    if (!m_imageItem) {
        m_imageItem = std::make_unique<ImageItem>(this);
    }
    m_imageItem->setImage(m_source->image());
    m_imageItem->setPosition(-m_source->hotspot());
    m_imageItem->setSize(m_source->image().deviceIndependentSize());
}

ShakeCursorEffect::ShakeCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    m_deflateTimer.setSingleShot(true);
    connect(&m_deflateTimer, &QTimer::timeout, this, &ShakeCursorEffect::deflate);

    connect(&m_scaleAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        magnify(m_scaleAnimation.currentValue().toReal());
    });

    ShakeCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

ShakeCursorEffect::~ShakeCursorEffect()
{
    magnify(1.0);
}

bool ShakeCursorEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool ShakeCursorEffect::isActive() const
{
    return m_currentMagnification != 1.0 || m_useShader;
}

void ShakeCursorEffect::reconfigure(ReconfigureFlags flags)
{
    ShakeCursorConfig::self()->read();

    m_shakeDetector.setInterval(ShakeCursorConfig::timeInterval());
    m_shakeDetector.setSensitivity(ShakeCursorConfig::sensitivity());
}

void ShakeCursorEffect::inflate()
{
    qreal magnification;
    if (m_targetMagnification == 1.0) {
        magnification = ShakeCursorConfig::magnification();
    } else {
        magnification = m_targetMagnification + ShakeCursorConfig::overMagnification();
    }

    animateTo(magnification);
}

void ShakeCursorEffect::deflate()
{
    if (m_useShader) {
        effects->addRepaintFull();
        m_useShader = false;
    }
    animateTo(1.0);
}

void ShakeCursorEffect::animateTo(qreal magnification)
{
    if (m_targetMagnification != magnification) {
        m_scaleAnimation.stop();

        m_scaleAnimation.setStartValue(m_currentMagnification);
        m_scaleAnimation.setEndValue(magnification);
        m_scaleAnimation.setDuration(200); // ignore animation speed, it's not an animation from user perspective
        m_scaleAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        m_scaleAnimation.start();

        m_targetMagnification = magnification;
    }
}

void ShakeCursorEffect::pointerMotion(PointerMotionEvent *event)
{
    if (event->buttons != Qt::NoButton || event->warp) {
        m_shakeDetector.reset();
        return;
    }

    if (input()->pointer()->isConstrained()) {
        return;
    }

    if (m_shakeDetector.update(event) && !m_useShader) {
        inflate();
        m_deflateTimer.start(2000);
    }
}

void ShakeCursorEffect::magnify(qreal magnification)
{
    if (m_useShader) {
        return;
    }
    if (magnification == 1.0) {
        m_currentMagnification = 1.0;
        if (m_cursorItem) {
            m_cursorItem.reset();
            effects->showCursor();
        }
    } else {
        m_currentMagnification = magnification;

        if (!m_cursorItem) {
            effects->hideCursor();
        }

        const qreal prefetchDevicePixelRatio = ShakeCursorConfig::magnification() + 8 * ShakeCursorConfig::overMagnification();
        const qreal devicePixelRatio = std::ceil(magnification / prefetchDevicePixelRatio) * prefetchDevicePixelRatio;
        if (!m_cursorItem || m_cursorTheme.devicePixelRatio() != devicePixelRatio) {
            const CursorTheme originalTheme = input()->pointer()->cursorTheme();
            if (m_cursorTheme.name() != originalTheme.name() || m_cursorTheme.size() != originalTheme.size() || m_cursorTheme.devicePixelRatio() != devicePixelRatio) {
                m_cursorTheme = CursorTheme(originalTheme.name(), originalTheme.size(), devicePixelRatio);
            }

            m_cursorItem = std::make_unique<ShakeCursorItem>(m_cursorTheme, effects->scene()->overlayItem());
            m_cursorItem->setPosition(m_cursor->pos());
            connect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), [this]() {
                m_cursorItem->setPosition(m_cursor->pos());
            });
        }
        m_cursorItem->setTransform(QTransform::fromScale(magnification, magnification));
    }
    if (m_currentMagnification >= 10) {
        m_useShader = true;
        effects->addRepaintFull();
        m_blackHoleStartMagnification = m_currentMagnification;
        m_blackHoleSize = 10;
        m_whiteHoleSize = 0;
        m_blackHoleStartPosition = m_cursorItem->position();
        m_blackHolePosition = m_cursorItem->position() + QPointF(5, 10) * m_currentMagnification;
        disconnect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), nullptr);
        effects->showCursor();
    }
}

bool ShakeCursorEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (!m_resourcesInit) {
        m_resourcesInit = true;
        ensureResources();
    }
    if (!m_useShader) {
        m_positionsTexture.reset();
        return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }
    m_deflateTimer.stop();

    if (!m_offscreenTexture || m_offscreenTexture->size() != renderTarget.size()) {
        m_offscreenTexture = GLTexture::allocate(GL_RGBA16, renderTarget.size());
        if (!m_offscreenTexture) {
            return false;
        }
        m_offscreenTexture->setWrapMode(GL_CLAMP_TO_BORDER);
        m_offscreenFb = std::make_unique<GLFramebuffer>(m_offscreenTexture.get());
        if (!m_offscreenFb->valid()) {
            m_offscreenTexture.reset();
            m_offscreenFb.reset();
            return false;
        }
    }

    RenderTarget offscreen(m_offscreenFb.get(), renderTarget.colorDescription());
    GLFramebuffer::pushFramebuffer(m_offscreenFb.get());
    if (!effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen)) {
        return false;
    }
    GLFramebuffer::popFramebuffer();

    if (!m_blackHoleShader) {
        m_blackHoleShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace, QByteArray(),
                                                                              QStringLiteral(":/effects/shakecursor/black_hole.frag"));
        if (!m_blackHoleShader) {
            deflate();
            return false;
        }
    }
    if (!m_blackHolePhysicsShader) {
        m_blackHolePhysicsShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QByteArray(),
                                                                                     QStringLiteral(":/effects/shakecursor/black_hole_physics.frag"));
        if (!m_blackHolePhysicsShader) {
            deflate();
            return false;
        }
    }
    if (!m_blackHoleInitShader) {
        m_blackHoleInitShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QByteArray(),
                                                                                  QStringLiteral(":/effects/shakecursor/black_hole_init.frag"));
        if (!m_blackHoleInitShader) {
            deflate();
            return false;
        }
    }

    if (!m_positionsTexture || m_positionsTexture->size() != renderTarget.size()) {
        m_positionsTexture = GLTexture::allocate(GL_RGBA32F, renderTarget.size());
        if (!m_positionsTexture) {
            return false;
        }
        m_positionsFb = std::make_unique<GLFramebuffer>(m_positionsTexture.get());
        if (!m_positionsFb->valid()) {
            m_positionsTexture.reset();
            m_positionsFb.reset();
            return false;
        }

        GLFramebuffer::pushFramebuffer(m_positionsFb.get());

        ShaderBinder binder(m_blackHoleInitShader.get());
        QMatrix4x4 proj;
        proj.ortho(QRect(QPoint(), m_offscreenTexture->size()));
        m_blackHoleInitShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);
        m_blackHoleInitShader->setUniform("size", QVector2D(renderTarget.size().width(), renderTarget.size().height()));
        m_offscreenTexture->render(m_offscreenTexture->size());

        GLFramebuffer::popFramebuffer();
    }

    const auto relativeCursor = m_blackHolePosition * viewport.scale() - viewport.deviceRect().topLeft();

    {
        // move pixels
        ShaderBinder binder(m_blackHolePhysicsShader.get());
        GLFramebuffer::pushFramebuffer(m_positionsFb.get());

        QMatrix4x4 proj;
        proj.ortho(QRect(QPoint(), m_offscreenTexture->size()));
        m_blackHolePhysicsShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);

        m_blackHolePhysicsShader->setUniform("blackHolePosition", QVector3D(relativeCursor.x(), relativeCursor.y(), 0));
        m_blackHolePhysicsShader->setUniform("size", QVector2D(renderTarget.size().width(), renderTarget.size().height()));
        m_blackHolePhysicsShader->setUniform("diameter", m_blackHoleSize);
        m_blackHolePhysicsShader->setUniform("timestep", 1'000.0 / screen->refreshRate());

        m_positionsTexture->render(deviceRegion, m_positionsTexture->size(), true);

        m_positionsTexture->bind();
        glTextureBarrierNV();
        m_positionsTexture->unbind();

        GLFramebuffer::popFramebuffer();
    }

    // render the result

    ShaderBinder binder(m_blackHoleShader.get());
    QMatrix4x4 proj = renderTarget.transform().toMatrix();
    proj.scale(1, -1);
    proj.ortho(QRect(QPoint(), m_offscreenTexture->size()));
    m_blackHoleShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);
    m_blackHoleShader->setColorspaceUniforms(renderTarget.colorDescription(), renderTarget.colorDescription(), RenderingIntent::Perceptual);

    m_blackHoleShader->setUniform("blackHolePosition", QVector3D(relativeCursor.x(), relativeCursor.y(), 0));
    m_blackHoleShader->setUniform("size", QVector2D(renderTarget.size().width(), renderTarget.size().height()));
    m_blackHoleShader->setUniform("diameter", m_blackHoleSize);
    m_blackHoleShader->setUniform("whiteHoleDiameter", m_whiteHoleSize);

    m_blackHoleShader->setUniform("positions", 1);
    glActiveTexture(GL_TEXTURE1);
    m_positionsTexture->bind();

    m_blackHoleShader->setUniform("screen", 0);
    glActiveTexture(GL_TEXTURE0);

    m_offscreenTexture->render(deviceRegion, m_offscreenTexture->size(), true);

    m_blackHoleSize += 200'000 / screen->refreshRate();
    m_blackHoleSize *= 1.0 + 35.0 / screen->refreshRate();

    m_currentMagnification = std::max(1.0, m_currentMagnification - 10'000.0 / screen->refreshRate());
    if (m_currentMagnification > 1) {
        m_cursorItem->setTransform(QTransform::fromScale(m_currentMagnification, m_currentMagnification));
        m_cursorItem->setPosition(QPointF{
            std::lerp(m_blackHolePosition.x(), m_blackHoleStartPosition.x(), m_currentMagnification / m_blackHoleStartMagnification),
            std::lerp(m_blackHolePosition.y(), m_blackHoleStartPosition.y(), m_currentMagnification / m_blackHoleStartMagnification),
        });
    } else {
        m_cursorItem.reset();
    }

    if (m_blackHoleSize > 2'500) {
        if (m_whiteHoleSize < 20) {
            m_whiteHoleSize = std::max(m_whiteHoleSize, 10.0);
            m_whiteHoleSize += 1'000'000 / screen->refreshRate();
        } else {
            m_whiteHoleSize += 500'000 / screen->refreshRate();
            m_whiteHoleSize *= 1.0 + 1'000.0 / screen->refreshRate();
        }
    }
    if (m_whiteHoleSize >= 2'500) {
        deflate();
    }

    return true;
}

void ShakeCursorEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_useShader) {
        effects->addRepaintFull();
    }
}

} // namespace KWin

#include "moc_shakecursor.cpp"
