/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclick.h"
// KConfigSkeleton
#include "mouseclickconfig.h"

#include "core/output.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"

#include <QAction>
#include <QDebug>
#include <QMatrix4x4>

#include <KConfigGroup>
#include <KGlobalAccel>

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(mouseclick);
}

namespace KWin
{

MouseClickEffect::MouseClickEffect()
{
    MouseClickConfig::instance(effects->config());
    m_enabled = false;
    toggleEnabled();
    ensureResources();
    loadShaders();

    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("ToggleMouseClick"));
    a->setText(i18n("Toggle Mouse Click Animation"));
    KGlobalAccel::self()->setGlobalShortcut(a, QKeySequence(Qt::META | Qt::Key_Asterisk));
    connect(a, &QAction::triggered, this, &MouseClickEffect::toggleEnabled);

    reconfigure(ReconfigureAll);

    m_buttons[0] = std::make_unique<MouseButton>(i18nc("Left mouse button", "Left"), Qt::LeftButton);
    m_buttons[1] = std::make_unique<MouseButton>(i18nc("Middle mouse button", "Middle"), Qt::MiddleButton);
    m_buttons[2] = std::make_unique<MouseButton>(i18nc("Right mouse button", "Right"), Qt::RightButton);
}

MouseClickEffect::~MouseClickEffect()
{
}

void MouseClickEffect::reconfigure(ReconfigureFlags)
{
    MouseClickConfig::self()->read();
    m_colors[0] = MouseClickConfig::color1();
    m_colors[1] = MouseClickConfig::color2();
    m_colors[2] = MouseClickConfig::color3();
    m_lineWidth = MouseClickConfig::lineWidth();
    m_ringLife = MouseClickConfig::ringLife();
    m_ringMaxSize = MouseClickConfig::ringSize();
    m_ringCount = MouseClickConfig::ringCount();
    m_showText = MouseClickConfig::showText();
    m_font = MouseClickConfig::font();
}

void MouseClickEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const int time = m_clock.tick(data.view).count();
    if (m_click) {
        m_click->m_time += time;
    }
}

bool MouseClickEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (!m_enabled) {
        return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }
    loadShaders();
    if (!m_shader || !m_feedbackShader) {
        return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }

    const QSize nativeSize = screen->geometry().size() * screen->scale();
    const GLenum textureFormat = renderTarget.texture() ? renderTarget.texture()->internalFormat() : GL_RGBA8;
    auto &capture = m_screenCaptures[screen];
    if (!capture.scene.texture || capture.scene.texture->size() != nativeSize || capture.scene.texture->internalFormat() != textureFormat) {
        capture = {};
        capture.scene.texture = GLTexture::allocate(textureFormat, nativeSize);
        if (!capture.scene.texture) {
            m_screenCaptures.remove(screen);
            return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
        }
        capture.scene.framebuffer = std::make_shared<GLFramebuffer>(capture.scene.texture.get());
        if (!capture.scene.framebuffer->valid()) {
            m_screenCaptures.remove(screen);
            return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
        }
        GLFramebuffer::pushFramebuffer(capture.scene.framebuffer.get());
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        GLFramebuffer::popFramebuffer();
        for (OffscreenTarget &feedback : capture.feedback) {
            feedback.texture = GLTexture::allocate(textureFormat, nativeSize);
            if (!feedback.texture) {
                m_screenCaptures.remove(screen);
                return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
            }
            feedback.framebuffer = std::make_shared<GLFramebuffer>(feedback.texture.get());
            if (!feedback.framebuffer->valid()) {
                m_screenCaptures.remove(screen);
                return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
            }

            GLFramebuffer::pushFramebuffer(feedback.framebuffer.get());
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            GLFramebuffer::popFramebuffer();
        }
    }

    // Render all effects below this one into an offscreen texture first.
    RenderTarget captureTarget(capture.scene.framebuffer.get(), renderTarget.colorDescription());
    RenderViewport captureViewport(viewport.renderRect(), viewport.scale(), captureTarget, QPoint());
    GLFramebuffer::pushFramebuffer(capture.scene.framebuffer.get());
    const bool painted = effects->paintScreen(captureTarget, captureViewport, mask, deviceRegion, screen);
    GLFramebuffer::popFramebuffer();
    if (!painted) {
        return false;
    }

    const RectF screenRect = screen->geometry();
    const QPointF cursor = cursorPos();
    const QVector2D cursorPosition((cursor.x() - screenRect.x()) / screenRect.width(),
                                   1.0 - (cursor.y() - screenRect.y()) / screenRect.height());
    QVector2D cursorRadius(8.0 * viewport.scale() / nativeSize.width(),
                           8.0 * viewport.scale() / nativeSize.height());

    if (!m_click || !m_click->m_press) {
        // cursorRadius = QVector2D(0.0, 0.0);
    }

    const int nextFrame = 1 - capture.previousFrame;
    const OffscreenTarget &previousFrame = capture.feedback[capture.previousFrame];
    const OffscreenTarget &nextFeedback = capture.feedback[nextFrame];
    GLFramebuffer::pushFramebuffer(nextFeedback.framebuffer.get());
    glActiveTexture(GL_TEXTURE1);
    previousFrame.texture->bind();
    glActiveTexture(GL_TEXTURE0);
    ShaderManager::instance()->pushShader(m_feedbackShader.get());
    QMatrix4x4 feedbackProjectionMatrix;
    feedbackProjectionMatrix.ortho(QRectF(QPointF(), nativeSize));
    m_feedbackShader->setUniform(m_feedbackModelViewProjectionMatrixLocation, feedbackProjectionMatrix);
    m_feedbackShader->setUniform("sampler", 0);
    m_feedbackShader->setUniform(m_feedbackTimeLocation, int(m_click ? m_click->m_time : 0));
    m_feedbackShader->setUniform(m_feedbackPreviousFrameLocation, 1);
    m_feedbackShader->setUniform(m_feedbackCursorPositionLocation, cursorPosition);
    m_feedbackShader->setUniform(m_cursorRadiusLocation, cursorRadius);
    m_feedbackShader->setUniform(m_feedbackScreenSizeLocation, QVector2D(nativeSize.width(), nativeSize.height()));
    capture.scene.texture->render(nativeSize);
    ShaderManager::instance()->popShader();
    glActiveTexture(GL_TEXTURE1);
    previousFrame.texture->unbind();
    glActiveTexture(GL_TEXTURE0);
    GLFramebuffer::popFramebuffer();
    capture.previousFrame = nextFrame;

    glActiveTexture(GL_TEXTURE0);
    const OffscreenTarget &feedbackFrame = capture.feedback[capture.previousFrame];
    glActiveTexture(GL_TEXTURE1);
    feedbackFrame.texture->bind();
    glActiveTexture(GL_TEXTURE0);
    ShaderManager::instance()->pushShader(m_shader.get());
    QMatrix4x4 modelViewProjectionMatrix(viewport.projectionMatrix());
    modelViewProjectionMatrix.translate(screenRect.x() * viewport.scale(), screenRect.y() * viewport.scale());
    m_shader->setUniform(m_modelViewProjectionMatrixLocation, modelViewProjectionMatrix);
    m_shader->setUniform("sampler", 0);
    m_shader->setUniform("feedbackTexture", 1);

    QColor color = Qt::transparent;
    if (m_click && m_click->m_press) {
        color = m_colors[m_click->m_button];
    }

    m_shader->setUniform(m_cursorColorLocation, QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
    m_shader->setUniform(m_cursorPositionLocation, cursorPosition);
    // m_shader->setUniform(m_cursorRadiusLocation, cursorRadius);
    capture.scene.texture->render(nativeSize);
    ShaderManager::instance()->popShader();
    glActiveTexture(GL_TEXTURE1);
    feedbackFrame.texture->unbind();
    glActiveTexture(GL_TEXTURE0);

    return true;
}

void MouseClickEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_enabled) {
        effects->addRepaintFull();
    }
}

void MouseClickEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                        Qt::MouseButtons buttons, Qt::MouseButtons oldButtons,
                                        Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (buttons == oldButtons) {
        return;
    }

    std::unique_ptr<MouseClickMouseEvent> m;
    int i = BUTTON_COUNT;
    while (--i >= 0) {
        MouseButton *b = m_buttons[i].get();
        if (isPressed(b->m_button, buttons, oldButtons)) {
            m = std::make_unique<MouseClickMouseEvent>(i, pos.toPoint(), 0, createEffectFrame(pos.toPoint(), b->m_labelDown), true);
            m_clock.reset();
            break;
        } else if (isReleased(b->m_button, buttons, oldButtons)) {
            // we might miss a press, thus also check !b->m_isPressed, bug #314762
            m = std::make_unique<MouseClickMouseEvent>(i, pos.toPoint(), 0, createEffectFrame(pos.toPoint(), b->m_labelUp), false);
            m_clock.reset();
            break;
        }
        b->setPressed(b->m_button & buttons);
    }

    if (m) {
        m_click = std::move(m);
    }
}

std::unique_ptr<EffectFrame> MouseClickEffect::createEffectFrame(const QPoint &pos, const QString &text)
{
    if (!m_showText) {
        return nullptr;
    }
    QPoint point(pos.x() + m_ringMaxSize, pos.y());
    std::unique_ptr<EffectFrame> frame = std::make_unique<EffectFrame>(EffectFrameStyled, false, point, Qt::AlignLeft);
    frame->setFont(m_font);
    frame->setText(text);
    return frame;
}

bool MouseClickEffect::isReleased(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons)
{
    return !(button & buttons) && (button & oldButtons);
}

bool MouseClickEffect::isPressed(Qt::MouseButtons button, Qt::MouseButtons buttons, Qt::MouseButtons oldButtons)
{
    return (button & buttons) && !(button & oldButtons);
}

void MouseClickEffect::toggleEnabled()
{
    m_enabled = !m_enabled;

    if (m_enabled) {
        connect(effects, &EffectsHandler::mouseChanged, this, &MouseClickEffect::slotMouseChanged);
    } else {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &MouseClickEffect::slotMouseChanged);
    }

    //    m_click.clear();

    effects->addRepaintFull();
}

bool MouseClickEffect::isActive() const
{
    return m_enabled;
}

void MouseClickEffect::loadShaders()
{
    if (!m_shader) {
        m_shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                     QStringLiteral(":/effects/mouseclick/shaders/mouseclick.vert"),
                                                                     QStringLiteral(":/effects/mouseclick/shaders/mouseclick.frag"));
        if (!m_shader) {
            qWarning("Failed to load the mouseclick shader");
        } else {
            m_modelViewProjectionMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
            m_cursorColorLocation = m_shader->uniformLocation("cursorColor");
            m_cursorPositionLocation = m_shader->uniformLocation("cursorPosition");
            // m_cursorRadiusLocation = m_shader->uniformLocation("cursorRadius");
        }
    }

    if (!m_feedbackShader) {
        m_feedbackShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                             QStringLiteral(":/effects/mouseclick/shaders/mouseclick.vert"),
                                                                             QStringLiteral(":/effects/mouseclick/shaders/feedback.frag"));
        if (!m_feedbackShader) {
            qWarning("Failed to load the mouseclick feedback shader");
        } else {
            m_feedbackModelViewProjectionMatrixLocation = m_feedbackShader->uniformLocation("modelViewProjectionMatrix");
            m_feedbackTimeLocation = m_feedbackShader->uniformLocation("time");
            m_feedbackCursorPositionLocation = m_feedbackShader->uniformLocation("cursorPosition");
            m_cursorRadiusLocation = m_feedbackShader->uniformLocation("cursorRadius");
            m_feedbackPreviousFrameLocation = m_feedbackShader->uniformLocation("previousFrame");
            m_feedbackScreenSizeLocation = m_feedbackShader->uniformLocation("screenSize");
        }
    }
}

QColor MouseClickEffect::color1() const
{
    return m_colors[0];
}

QColor MouseClickEffect::color2() const
{
    return m_colors[1];
}

QColor MouseClickEffect::color3() const
{
    return m_colors[2];
}

qreal MouseClickEffect::lineWidth() const
{
    return m_lineWidth;
}

int MouseClickEffect::ringLife() const
{
    return m_ringLife;
}

int MouseClickEffect::ringSize() const
{
    return m_ringMaxSize;
}

int MouseClickEffect::ringCount() const
{
    return m_ringCount;
}

bool MouseClickEffect::isShowText() const
{
    return m_showText;
}

QFont MouseClickEffect::font() const
{
    return m_font;
}

bool MouseClickEffect::isEnabled() const
{
    return m_enabled;
}

} // namespace

#include "moc_mouseclick.cpp"
