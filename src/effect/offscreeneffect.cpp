/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/offscreeneffect.h"
#include "compositor.h"
#include "core/gpumanager.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/renderdevice.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/syncobjtimeline.h"
#include "effect/effecthandler.h"
#include "multigpuswapchain.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/windowitem.h"

namespace KWin
{

struct OffscreenData
{
public:
    virtual ~OffscreenData();
    void setDirty();
    void setShader(FragmentShaderInfo *shader);
    void setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode);

    [[nodiscard]] bool paint(RenderDevice *device, const RenderTarget &renderTarget, const RenderViewport &viewport,
                             EffectWindow *window, const Region &deviceRegion,
                             const WindowPaintData &data, const WindowQuadList &quads);

    [[nodiscard]] bool maybeRender(RenderDevice *device, EffectWindow *window);

    struct GLResources
    {
        ~GLResources();

        std::shared_ptr<EglContext> m_context;
        std::shared_ptr<EglSwapchain> m_swapchain;
        std::shared_ptr<EglSwapchainSlot> m_slot;
        std::shared_ptr<SyncReleasePoint> m_releasePoint;

        std::unique_ptr<MultiGpuSwapchain> m_mgpuSwapchain;
        std::shared_ptr<GLTexture> m_importedTexture;

        bool m_isDirty = true;
        std::unique_ptr<GLShader> m_shader;
        QString m_shaderPath;
        ShaderTraits m_shaderTraits;
    };
    std::unordered_map<RenderDevice *, GLResources> m_resources;

    FragmentShaderInfo *m_shaderInfo = nullptr;
    RenderGeometry::VertexSnappingMode m_vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
    QMetaObject::Connection m_windowDamagedConnection;
    ItemEffect m_windowEffect;
};

class OffscreenEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<OffscreenData>> windows;
    QMetaObject::Connection windowDeletedConnection;
    RenderGeometry::VertexSnappingMode vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
};

OffscreenEffect::OffscreenEffect(QObject *parent)
    : Effect(parent)
    , d(std::make_unique<OffscreenEffectPrivate>())
{
    connect(GpuManager::self(), &GpuManager::renderDeviceRemoved, this, [this](RenderDevice *removed) {
        for (auto &[window, data] : d->windows) {
            data->m_resources.erase(removed);
        }
    });
}

OffscreenEffect::~OffscreenEffect() = default;

bool OffscreenEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OffscreenEffect::redirect(EffectWindow *window)
{
    std::unique_ptr<OffscreenData> &offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }
    offscreenData = std::make_unique<OffscreenData>();
    offscreenData->setVertexSnappingMode(d->vertexSnappingMode);
    offscreenData->m_windowEffect = ItemEffect(window->windowItem());
    offscreenData->m_windowDamagedConnection =
        connect(window, &EffectWindow::windowDamaged, this, &OffscreenEffect::handleWindowDamaged);

    if (d->windows.size() == 1) {
        setupConnections();
    }
}

void OffscreenEffect::unredirect(EffectWindow *window)
{
    auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return;
    }

    d->windows.erase(it);
    if (d->windows.empty()) {
        destroyConnections();
    }
}

void OffscreenEffect::setShader(EffectWindow *window, FragmentShaderInfo *shader)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setShader(shader);
    }
}

void OffscreenEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
}

OffscreenData::GLResources::~GLResources()
{
    if (m_context) {
        (void)m_context->makeCurrent();
    }
}

bool OffscreenData::maybeRender(RenderDevice *device, EffectWindow *window)
{
    const qreal scale = window->screen()->scale();
    const RectF logicalGeometry = snapToPixels(window->expandedGeometry(), scale);
    const QSize textureSize = (logicalGeometry.size() * scale).toSize();

    if (textureSize.isEmpty()) {
        m_resources.erase(device);
        return EglContext::currentContext() != nullptr;
    }
    auto &resources = m_resources[device];
    if (!resources.m_context) {
        resources.m_context = device->eglContext();
        if (!resources.m_context) {
            return false;
        }
    }

    if (resources.m_mgpuSwapchain) {
        resources.m_importedTexture.reset();
        resources.m_mgpuSwapchain.reset();
        if (!EglContext::currentContext()) {
            return false;
        }
    }

    if (!resources.m_swapchain || resources.m_swapchain->size() != textureSize) {
        resources.m_slot.reset();

        const auto device = Compositor::self()->primaryDevice();
        GraphicsBufferOptions options{
            .size = textureSize,
            .format = DRM_FORMAT_ARGB8888,
            .modifiers = device->renderableFormats()[DRM_FORMAT_ARGB8888],
            .software = false,
            .scanout = false,
        };
        resources.m_swapchain = EglSwapchain::create(device, options);
        if (!resources.m_swapchain) {
            return false;
        }
        resources.m_isDirty = true;
    }

    if (!resources.m_isDirty) {
        return true;
    }
    resources.m_slot = resources.m_swapchain->acquire();
    if (!resources.m_slot) {
        return false;
    }
    RenderTarget renderTarget(resources.m_slot->framebuffer());
    RenderViewport viewport(logicalGeometry, scale, renderTarget, QPoint());
    GLFramebuffer::pushFramebuffer(resources.m_slot->framebuffer());
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    WindowPaintData data;
    data.setOpacity(1.0);

    const int mask = Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_WINDOW_TRANSLUCENT;
    if (!effects->drawWindow(device, renderTarget, viewport, window, mask, Region::infinite(), data)) {
        resources.m_slot.reset();
        resources.m_swapchain.reset();
        return false;
    }

    GLFramebuffer::popFramebuffer();
    EGLNativeFence fence(resources.m_swapchain->context()->displayObject());
    resources.m_swapchain->release(resources.m_slot, fence.takeFileDescriptor());
    resources.m_releasePoint = resources.m_slot->releasePoint();

    resources.m_isDirty = false;
    return true;
}

OffscreenData::~OffscreenData()
{
    QObject::disconnect(m_windowDamagedConnection);
}

void OffscreenData::setDirty()
{
    for (auto &[device, resource] : m_resources) {
        resource.m_isDirty = true;
    }
}

void OffscreenData::setShader(FragmentShaderInfo *shader)
{
    m_shaderInfo = shader;
}

void OffscreenData::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    m_vertexSnappingMode = mode;
}

bool OffscreenData::paint(RenderDevice *device, const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, const Region &deviceRegion,
                          const WindowPaintData &data, const WindowQuadList &quads)
{
    auto &resources = m_resources[device];
    GLTexture *texture = nullptr;

    if (resources.m_slot) {
        texture = resources.m_slot->texture().get();
    } else {
        // find a different device to copy from.
        // This can happen with the crossfade effect, which takes a
        // snapshot of the window on any GPU
        const auto it = std::ranges::find_if(m_resources, [](const auto &pair) {
            const auto &[device, resources] = pair;
            return resources.m_slot != nullptr;
        });
        if (it == m_resources.end()) {
            // Technically this means we failed, but rendering doesn't need to abort because of this
            return true;
        }

        const auto &[srcDevice, srcResources] = *it;
        GraphicsBuffer *srcBuffer = srcResources.m_slot->buffer();
        if (!resources.m_mgpuSwapchain || !resources.m_mgpuSwapchain->isSuitableFor(srcBuffer)) {
            resources.m_mgpuSwapchain = MultiGpuSwapchain::createForSampling(srcDevice, device,
                                                                             srcBuffer->dmabufAttributes()->format,
                                                                             srcBuffer->dmabufAttributes()->modifier,
                                                                             srcBuffer->size(),
                                                                             device->eglDisplay()->allSupportedDrmFormats());
        }
        if (!resources.m_mgpuSwapchain || !EglContext::currentContext()) {
            return false;
        }
        auto ret = resources.m_mgpuSwapchain->copyRgbBuffer(srcBuffer, Rect(QPoint(), srcBuffer->size()),
                                                            srcResources.m_slot->releaseFd().duplicate(), nullptr,
                                                            srcResources.m_slot->releasePoint());
        if (!ret || !EglContext::currentContext()) {
            return EglContext::currentContext() != nullptr;
        }

        EGLNativeFence fence = EGLNativeFence::importFence(device->eglDisplay(), std::move(ret->sync));
        fence.waitSync();

        if (!resources.m_context) {
            resources.m_context = device->eglContext();
        }
        if (!resources.m_context || !resources.m_context->makeCurrent()) {
            return false;
        }

        resources.m_importedTexture = resources.m_context->importDmaBufAsTexture(*ret->buffer->dmabufAttributes());
        texture = resources.m_importedTexture.get();
        resources.m_releasePoint = ret->releasePoint;
        if (!texture) {
            return true;
        }
    }

    const QString shaderPath = m_shaderInfo ? m_shaderInfo->m_path : QString();
    if (resources.m_shaderPath != shaderPath) {
        resources.m_shaderPath = shaderPath;
        if (shaderPath.isEmpty()) {
            resources.m_shader.reset();
        } else {
            resources.m_shaderTraits = m_shaderInfo->m_traits;
            resources.m_shader = ShaderManager::instance()->generateShaderFromFile(resources.m_shaderTraits, {}, shaderPath);
        }
    }

    GLShader *shader = resources.m_shader ? resources.m_shader.get() : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation | ShaderTrait::TransformColorspace);
    ShaderBinder binder(shader);

    const double scale = viewport.scale();

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    RenderGeometry geometry;
    geometry.setVertexSnappingMode(m_vertexSnappingMode);
    for (auto &quad : quads) {
        geometry.appendWindowQuad(quad, scale);
    }
    geometry.postProcessTextureCoordinates(texture->matrix(NormalizedCoordinates));

    const auto map = vbo->map<GLVertex2D>(geometry.size());
    if (!map) {
        return false;
    }
    geometry.copy(*map);
    vbo->unmap();

    vbo->bindArrays();

    const qreal rgb = data.brightness() * data.opacity();
    const qreal a = data.opacity();

    QMatrix4x4 mvp = viewport.projectionMatrix();
    mvp.translate(std::round(window->x() * scale), std::round(window->y() * scale));

    const auto toXYZ = renderTarget.colorDescription()->containerColorimetry().toXYZ();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp * data.toMatrix(scale));
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::FloatUniform::Saturation, data.saturation());
    shader->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));
    shader->setUniform(GLShader::IntUniform::TextureWidth, texture->width());
    shader->setUniform(GLShader::IntUniform::TextureHeight, texture->height());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    const bool clipping = deviceRegion != Region::infinite();
    const Region clipRegion = clipping ? viewport.transform().map(deviceRegion, renderTarget.transformedSize()) : Region::infinite();

    if (clipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    texture->bind();
    vbo->draw(clipRegion, GL_TRIANGLES, 0, geometry.count(), clipping);
    texture->unbind();

    glDisable(GL_BLEND);
    if (clipping) {
        glDisable(GL_SCISSOR_TEST);
    }
    vbo->unbindArrays();

    if (resources.m_releasePoint) {
        EGLNativeFence fence(resources.m_context->displayObject());
        resources.m_releasePoint->addReleaseFence(fence.takeFileDescriptor());
    }

    return true;
}

bool OffscreenEffect::drawWindow(RenderDevice *device, const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    const auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return effects->drawWindow(device, renderTarget, viewport, window, mask, deviceRegion, data);
    }
    OffscreenData *offscreenData = it->second.get();

    const RectF expandedGeometry = snapToPixels(window->expandedGeometry(), viewport.scale());
    const RectF frameGeometry = snapToPixels(window->frameGeometry(), viewport.scale());

    RectF visibleRect = expandedGeometry;
    visibleRect.moveTopLeft(expandedGeometry.topLeft() - frameGeometry.topLeft());
    WindowQuad quad;
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    apply(window, mask, data, quads);

    return offscreenData->maybeRender(device, window)
        && offscreenData->paint(device, renderTarget, viewport, window, deviceRegion, data, quads);
}

void OffscreenEffect::handleWindowDamaged(EffectWindow *window)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setDirty();
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    d->windowDeletedConnection =
        connect(effects, &EffectsHandler::windowDeleted, this, &OffscreenEffect::handleWindowDeleted);
}

void OffscreenEffect::destroyConnections()
{
    disconnect(d->windowDeletedConnection);

    d->windowDeletedConnection = {};
}

void OffscreenEffect::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    d->vertexSnappingMode = mode;
    for (auto &window : std::as_const(d->windows)) {
        window.second->setVertexSnappingMode(mode);
    }
}

bool OffscreenEffect::blocksDirectScanout() const
{
    return false;
}

class CrossFadeWindowData : public OffscreenData
{
public:
    RectF frameGeometryAtCapture;
};

class CrossFadeEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<CrossFadeWindowData>> windows;
    qreal progress;
};

CrossFadeEffect::CrossFadeEffect(QObject *parent)
    : Effect(parent)
    , d(std::make_unique<CrossFadeEffectPrivate>())
{
}

CrossFadeEffect::~CrossFadeEffect() = default;

bool CrossFadeEffect::drawWindow(RenderDevice *device, const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    const auto it = d->windows.find(window);

    // paint the new window (if applicable) underneath
    if (data.crossFadeProgress() > 0 || it == d->windows.end()) {
        if (!Effect::drawWindow(device, renderTarget, viewport, window, mask, deviceRegion, data)) {
            return false;
        }
    }

    if (it == d->windows.end()) {
        return true;
    }
    CrossFadeWindowData *offscreenData = it->second.get();

    // paint old snapshot on top
    WindowPaintData previousWindowData = data;
    previousWindowData.setOpacity((1.0 - data.crossFadeProgress()) * data.opacity());

    const RectF expandedGeometry = snapToPixels(window->expandedGeometry(), viewport.scale());
    const RectF frameGeometry = snapToPixels(window->frameGeometry(), viewport.scale());

    // This is for the case of *non* live effect, when the window buffer we saved has a different size
    // compared to the size the window has now. The "old" window will be rendered scaled to the current
    // window geometry, but everything will be scaled, also the shadow if there is any, making the window
    // frame not line up anymore with window->frameGeometry()
    // to fix that, we consider how much the shadow will have scaled, and use that as margins to the
    // current frame geometry. this causes the scaled window to visually line up perfectly with frameGeometry,
    // having the scaled shadow all outside of it.
    const qreal widthRatio = offscreenData->frameGeometryAtCapture.width() / frameGeometry.width();
    const qreal heightRatio = offscreenData->frameGeometryAtCapture.height() / frameGeometry.height();

    const QMarginsF margins(
        (expandedGeometry.x() - frameGeometry.x()) / widthRatio,
        (expandedGeometry.y() - frameGeometry.y()) / heightRatio,
        (frameGeometry.right() - expandedGeometry.right()) / widthRatio,
        (frameGeometry.bottom() - expandedGeometry.bottom()) / heightRatio);

    RectF visibleRect = RectF(QPointF(0, 0), frameGeometry.size()) - margins;

    WindowQuad quad;
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    return offscreenData->paint(device, renderTarget, viewport, window, deviceRegion, previousWindowData, quads);
}

void CrossFadeEffect::redirect(EffectWindow *window)
{
    if (d->windows.empty()) {
        connect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }

    std::unique_ptr<CrossFadeWindowData> &offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }
    offscreenData = std::make_unique<CrossFadeWindowData>();
    offscreenData->m_windowEffect = ItemEffect(window->windowItem());

    // Avoid including blur and contrast effects. During a normal painting cycle they
    // won't be included, but since we call effects->drawWindow() outside usual compositing
    // cycle, we have to prevent backdrop effects kicking in.
    const QVariant blurRole = window->data(WindowForceBlurRole);
    window->setData(WindowForceBlurRole, QVariant());
    const QVariant contrastRole = window->data(WindowForceBackgroundContrastRole);
    window->setData(WindowForceBackgroundContrastRole, QVariant());

    const auto device = Compositor::self()->primaryDevice();
    const auto context = device->eglContext();
    if (!context || !context->makeCurrent()) {
        return;
    }
    (void)offscreenData->maybeRender(device, window);
    offscreenData->frameGeometryAtCapture = window->frameGeometry();

    window->setData(WindowForceBlurRole, blurRole);
    window->setData(WindowForceBackgroundContrastRole, contrastRole);
}

void CrossFadeEffect::unredirect(EffectWindow *window)
{
    auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return;
    }

    d->windows.erase(it);
    if (d->windows.empty()) {
        disconnect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }
}

void CrossFadeEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void CrossFadeEffect::setShader(EffectWindow *window, FragmentShaderInfo *shader)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setShader(shader);
    }
}

bool CrossFadeEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_offscreeneffect.cpp"
