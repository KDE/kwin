/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "screentransform.h"
#include "compositor.h"
#include "core/gpumanager.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderdevice.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "main.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <QDebug>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(KWIN_SCREENTRANSFORM, "kwin_effect_screentransform", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(screentransform);
}

namespace KWin
{

ScreenTransformEffect::ScreenTransformEffect()
    : Effect()
{
    // Make sure that shaders in /effects/screentransform/shaders/* are loaded.
    ensureResources();

    const auto outputs = kwinApp()->outputBackend()->outputs();
    for (BackendOutput *output : outputs) {
        addOutput(output);
    }
    connect(kwinApp()->outputBackend(), &OutputBackend::outputAdded, this, &ScreenTransformEffect::addOutput);
    connect(kwinApp()->outputBackend(), &OutputBackend::outputRemoved, this, &ScreenTransformEffect::removeOutput);
    connect(GpuManager::self(), &GpuManager::renderDeviceRemoved, this, &ScreenTransformEffect::removeRenderDevice);
}

ScreenTransformEffect::~ScreenTransformEffect() = default;

bool ScreenTransformEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->animationsSupported();
}

qreal transformAngle(OutputTransform current, OutputTransform old)
{
    auto ensureShort = [](int angle) {
        return angle > 180 ? angle - 360 : angle < -180 ? angle + 360
                                                        : angle;
    };
    // % 4 to ignore flipped cases (for now)
    return ensureShort((int(current.kind()) % 4 - int(old.kind()) % 4) * 90);
}

void ScreenTransformEffect::addOutput(BackendOutput *output)
{
    connect(output, &BackendOutput::aboutToChange, this, [this, output](OutputChangeSet *changeSet) {
        const OutputTransform transform = changeSet->transform.value_or(output->transform());
        if (output->transform() == transform) {
            return;
        }

        SceneView *view = Compositor::self()->sceneView(output);
        if (!view) {
            return;
        }
        const auto context = view->renderDevice()->eglContext();
        if (!context || !context->makeCurrent()) {
            m_states.erase(output);
            return;
        }

        const auto logicalOutput = workspace()->findOutput(output);

        // Avoid including this effect while capturing previous screen state.
        m_capturing = true;
        auto resetCapturing = qScopeGuard([this]() {
            m_capturing = false;
        });

        auto texture = GLTexture::allocate(GL_RGBA16F, output->pixelSize());
        if (!texture) {
            m_states.erase(output);
            return;
        }

        auto shader = ShaderManager::instance()->generateShaderFromFile(
            ShaderTrait::MapTexture,
            QStringLiteral(":/effects/screentransform/shaders/crossfade.vert"),
            QStringLiteral(":/effects/screentransform/shaders/crossfade.frag"));
        if (!shader) {
            qCCritical(KWIN_SCREENTRANSFORM) << "Failed to load the crossfade shader.";
            m_states.erase(output);
            return;
        }

        auto &state = m_states[output];
        state.m_device = view->renderDevice();
        state.m_context = context;
        state.m_shader = std::move(shader);
        state.m_modelViewProjectioMatrixLocation = state.m_shader->uniformLocation("modelViewProjectionMatrix");
        state.m_blendFactorLocation = state.m_shader->uniformLocation("blendFactor");
        state.m_previousTextureLocation = state.m_shader->uniformLocation("previousTexture");
        state.m_currentTextureLocation = state.m_shader->uniformLocation("currentTexture");
        state.m_oldTransform = output->transform();
        state.m_oldGeometry = logicalOutput->geometry();
        state.m_timeLine.setDuration(animationTime(250ms));
        state.m_timeLine.setEasingCurve(QEasingCurve::InOutCubic);
        state.m_angle = transformAngle(changeSet->transform.value(), state.m_oldTransform);
        state.m_prev.texture = std::move(texture);
        state.m_prev.framebuffer = std::make_unique<GLFramebuffer>(state.m_prev.texture.get());
        RenderTarget renderTarget(state.m_prev.framebuffer.get(), output->blendingColor());

        Scene *scene = effects->scene();
        SceneView delegate(scene, logicalOutput, output, nullptr, view->renderDevice());
        delegate.setViewport(logicalOutput->geometryF());
        delegate.setScale(output->scale());
        scene->prePaint(&delegate);
        scene->paint(renderTarget, QPoint(), logicalOutput->geometry());
        scene->postPaint();
    });
}

void ScreenTransformEffect::removeOutput(BackendOutput *output)
{
    m_states.erase(output);
}

void ScreenTransformEffect::removeRenderDevice(RenderDevice *device)
{
    std::erase_if(m_states, [device](const auto &pair) {
        const auto &[output, state] = pair;
        return state.m_device == device;
    });
}

void ScreenTransformEffect::prePaintScreen(ScreenPrePaintData &data)
{
    m_currentView = data.view;
    auto it = m_states.find(m_currentView->backendOutput());
    if (it != m_states.end()) {
        auto &state = it->second;
        state.m_timeLine.advance(data.view);
        if (state.m_timeLine.done()) {
            m_states.erase(it);
        }
    }

    effects->prePaintScreen(data);
}

static GLVertexBuffer *texturedRectVbo(const RectF &geometry, qreal scale)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const auto opt = vbo->map<GLVertex2D>(6);
    if (!opt) {
        return nullptr;
    }
    const auto map = *opt;

    auto deviceGeometry = geometry.scaled(scale);

    // first triangle
    map[0] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[1] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };
    map[2] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.bottom()),
        .texcoord = QVector2D(0.0, 0.0),
    };

    // second triangle
    map[3] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[4] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.top()),
        .texcoord = QVector2D(1.0, 1.0),
    };
    map[5] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };

    vbo->unmap();
    return vbo;
}

static qreal lerp(qreal a, qreal b, qreal t)
{
    return (1 - t) * a + t * b;
}

static RectF lerp(const RectF &a, const RectF &b, qreal t)
{
    RectF ret;
    ret.setWidth(lerp(a.width(), b.width(), t));
    ret.setHeight(lerp(a.height(), b.height(), t));
    ret.moveCenter(b.center());
    return ret;
}

bool ScreenTransformEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    auto it = m_states.find(m_currentView->backendOutput());
    if (it == m_states.end() || m_currentView->backendOutput() != screen->backendOutput()) {
        return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }
    ScreenState &state = it->second;

    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->geometry().size() * screen->scale();
    if (!state.m_current.texture || state.m_current.texture->size() != nativeSize
        || state.m_current.texture->internalFormat() != renderTarget.texture()->internalFormat()) {
        state.m_current.texture = GLTexture::allocate(renderTarget.texture()->internalFormat(), nativeSize);
        if (!state.m_current.texture) {
            m_states.erase(m_currentView->backendOutput());
            EglContext::currentContext()->setFailed();
            return false;
        }
        state.m_current.framebuffer = std::make_unique<GLFramebuffer>(state.m_current.texture.get());
    }

    RenderTarget fboRenderTarget(state.m_current.framebuffer.get(), renderTarget.colorDescription());
    RenderViewport fboViewport(viewport.renderRect(), viewport.scale(), fboRenderTarget, QPoint());

    GLFramebuffer::pushFramebuffer(state.m_current.framebuffer.get());
    if (!effects->paintScreen(fboRenderTarget, fboViewport, mask, deviceRegion, screen)) {
        return false;
    }
    GLFramebuffer::popFramebuffer();

    const qreal blendFactor = state.m_timeLine.value();
    const RectF screenRect = screen->geometry();
    const qreal angle = state.m_angle * (1 - blendFactor);

    const auto scale = viewport.scale();

    // Projection matrix + rotate transform.
    const QVector3D transformOrigin(screenRect.center());
    QMatrix4x4 modelViewProjectionMatrix(viewport.projectionMatrix());
    modelViewProjectionMatrix.translate(transformOrigin * scale);
    modelViewProjectionMatrix.rotate(angle, 0, 0, 1);
    modelViewProjectionMatrix.translate(-transformOrigin * scale);

    glActiveTexture(GL_TEXTURE1);
    state.m_prev.texture->bind();
    glActiveTexture(GL_TEXTURE0);
    state.m_current.texture->bind();

    // Clear the background.
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLVertexBuffer *vbo = texturedRectVbo(lerp(state.m_oldGeometry, screenRect, blendFactor), scale);
    if (!vbo) {
        EglContext::currentContext()->setFailed();
        return false;
    }

    ShaderManager *sm = ShaderManager::instance();
    sm->pushShader(state.m_shader.get());
    state.m_shader->setUniform(state.m_modelViewProjectioMatrixLocation, modelViewProjectionMatrix);
    state.m_shader->setUniform(state.m_blendFactorLocation, float(blendFactor));
    state.m_shader->setUniform(state.m_currentTextureLocation, 0);
    state.m_shader->setUniform(state.m_previousTextureLocation, 1);

    vbo->bindArrays();
    vbo->draw(GL_TRIANGLES, 0, 6);
    vbo->unbindArrays();
    sm->popShader();

    glActiveTexture(GL_TEXTURE1);
    state.m_prev.texture->unbind();
    glActiveTexture(GL_TEXTURE0);
    state.m_current.texture->unbind();

    effects->addRepaintFull();
    return true;
}

bool ScreenTransformEffect::isActive() const
{
    return !m_states.empty() && !m_capturing;
}

ScreenTransformEffect::ScreenState::~ScreenState()
{
    if (m_context) {
        (void)m_context->makeCurrent();
    }
}

} // namespace KWin

#include "moc_screentransform.cpp"
