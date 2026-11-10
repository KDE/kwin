/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "offscreeneffectitem.h"
#include "compositor.h"
#include "core/pixelgrid.h"
#include "core/renderdevice.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/syncobjtimeline.h"
#include "effect/effect.h"
#include "main.h"
#include "multigpuswapchain.h"
#include "offscreeneffect.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/glframebuffer.h"
#include "scene/itemrenderer.h"
#include "scene/texture.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"

namespace KWin
{

SnapshotItem::SnapshotItem(EffectWindow *window, Item *parent, Item *toSnapshot,
                           const std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> &applyGeometryCallback)
    : Item(parent)
    , m_window(window)
    , m_snapshotItem(toSnapshot)
    , m_applyGeometryCallback(applyGeometryCallback)
{
    connect(window, &EffectWindow::windowDamaged, this, &SnapshotItem::scheduleUpdate);
    connect(window, &EffectWindow::windowOpacityChanged, this, &SnapshotItem::scheduleUpdate);
}

void SnapshotItem::freeze()
{
    maybeRender(kwinApp()->scene()->renderer(Compositor::self()->primaryDevice()));
    m_frozen = true;
}

void SnapshotItem::setItem(Item *toSnapshot)
{
    if (m_snapshotItem == toSnapshot) {
        return;
    }
    m_snapshotItem = toSnapshot;
    scheduleUpdate();
}

void SnapshotItem::preprocess(ItemRenderer *renderer)
{
    if (m_applyGeometryCallback) {
        discardQuads();
    }

    std::optional<bool> needsUpdate = false;
    if (!m_frozen) {
        needsUpdate = maybeRender(renderer);
        if (!needsUpdate.has_value()) {
            m_resources.clear();
            return;
        }
    }

    // In the frozen case, we may have a buffer from another device
    // that needs to be copied over.
    auto sourceIt = m_resources.find(renderer->renderDevice());
    if (sourceIt == m_resources.end()) {
        sourceIt = std::ranges::find_if(m_resources, [](const auto &pair) {
            return pair.second.m_buffer != nullptr;
        });
    }
    if (sourceIt == m_resources.end()) {
        return;
    }

    const auto &srcResources = sourceIt->second;
    auto &dstResources = m_resources[renderer->renderDevice()];
    if (dstResources.texture) {
        if (*needsUpdate) {
            dstResources.texture->attach(srcResources.m_buffer.get(), srcResources.m_bufferSync,
                                         Rect(QPoint(), dstResources.texture->size()),
                                         srcResources.m_releasePoint, srcResources.m_colorDescription);
        }
    } else {
        dstResources.texture = renderer->createTexture(srcResources.m_buffer.get(), srcResources.m_bufferSync,
                                                       srcResources.m_releasePoint, srcResources.m_colorDescription);
    }
}

std::optional<bool> SnapshotItem::maybeRender(ItemRenderer *renderer)
{
    const qreal scale = m_window->screen()->scale();
    const RectF logicalGeometry = snapToPixels(m_snapshotItem->boundingRect(), scale);
    const QSize textureSize = (logicalGeometry.size() * scale).toSize();

    if (textureSize.isEmpty()) {
        return std::nullopt;
    }
    auto &resources = m_resources[renderer->renderDevice()];

    // Capture the window in the color space the scene is blended in, so that a window with a wider
    // color description than sRGB keeps its colors while it is redirected.
    const std::shared_ptr<ColorDescription> &colorDescription = m_window->screen()->blendingColor();

    const auto device = Compositor::self()->primaryDevice();
    const FormatModifierMap &supportedFormats = device->eglDisplay()->nonExternalOnlySupportedDrmFormats();
    // An 8 bit intermediate bands, most visibly across an HDR blending space's luminance range, so
    // ask for 16 and let chooseFormat settle on what this device actually supports.
    const auto chosen = MultiGpuSwapchain::chooseFormat(DRM_FORMAT_ARGB16161616, supportedFormats, supportedFormats)
                            .value_or(DrmFormat{
                                .format = DRM_FORMAT_ARGB8888,
                                .modifiers = supportedFormats[DRM_FORMAT_ARGB8888],
                            });

    if (!resources.m_swapchain || resources.m_swapchain->size() != textureSize || resources.m_swapchain->format() != chosen.format) {
        GraphicsBufferOptions options{
            .size = textureSize,
            .format = chosen.format,
            .modifiers = chosen.modifiers,
            .software = device->isSoftwareDevice(),
            .scanout = false,
        };
        resources.m_swapchain = EglSwapchain::create(device, options);
        if (!resources.m_swapchain) {
            return false;
        }
        resources.m_dirty = true;
    }
    if (resources.m_colorDescription != colorDescription) {
        resources.m_colorDescription = colorDescription;
        resources.m_dirty = true;
    }

    if (!resources.m_dirty) {
        return false;
    }
    auto slot = resources.m_swapchain->acquire();
    if (!slot) {
        return false;
    }
    RenderTarget renderTarget(slot->framebuffer(), resources.m_colorDescription);
    RenderViewport viewport(logicalGeometry, scale, renderTarget, QPoint());
    GLFramebuffer::pushFramebuffer(slot->framebuffer());

    const auto filter = [this](Item *other) {
        return other == this;
    };

    const Region repaint = Rect(QPoint(), slot->framebuffer()->size());
    renderer->renderBackground(renderTarget, viewport, repaint);
    if (!renderer->renderItem(renderTarget, viewport, m_snapshotItem, 0, repaint, WindowPaintData{}, filter, {})) {
        return false;
    }

    GLFramebuffer::popFramebuffer();
    EGLNativeFence fence(resources.m_swapchain->context()->displayObject());

    resources.m_buffer = slot->buffer();
    resources.m_bufferSync = fence.fileDescriptor().duplicate();
    resources.m_releasePoint = slot->releasePoint();

    resources.m_swapchain->release(slot, fence.takeFileDescriptor());
    resources.m_dirty = false;
    return true;
}

void SnapshotItem::scheduleUpdate()
{
    for (auto &[device, resource] : m_resources) {
        resource.m_dirty = true;
    }
    scheduleRepaint(rect());
}

Texture *SnapshotItem::texture(RenderDevice *device) const
{
    const auto it = m_resources.find(device);
    return it == m_resources.end() ? nullptr : it->second.texture.get();
}

void SnapshotItem::releaseResources(RenderDevice *device)
{
    m_resources.erase(device);
}

void SnapshotItem::setShader(GLShader *shader)
{
    m_shader = shader;
}

GLShader *SnapshotItem::shader() const
{
    return m_shader;
}

WindowQuadList SnapshotItem::buildQuads(ItemRenderer *renderer)
{
    const RectF geoRect = rect();
    const auto tex = texture(renderer->renderDevice());
    if (geoRect.isEmpty() || !tex) {
        return WindowQuadList{};
    }
    const Rect texRect{QPoint(), tex->size()};

    WindowQuad quad;
    quad[0] = WindowVertex(geoRect.topLeft(), texRect.topLeft());
    quad[1] = WindowVertex(geoRect.topRight(), texRect.topRight());
    quad[2] = WindowVertex(geoRect.bottomRight(), texRect.bottomRight());
    quad[3] = WindowVertex(geoRect.bottomLeft(), texRect.bottomLeft());

    WindowQuadList ret;
    ret.append(quad);
    if (m_applyGeometryCallback) {
        m_applyGeometryCallback(m_window, this, ret);
    }
    return ret;
}

RegionF SnapshotItem::shape() const
{
    return rect();
}

RegionF SnapshotItem::opaque() const
{
    return RegionF{};
}

OffscreenEffectItem::OffscreenEffectItem(EffectWindow *window, const std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> &applyGeometryCallback)
    : TransformItem(applyGeometryCallback ? window->windowItem()->geometryContainer() : window->windowItem()->effectContainer())
    , m_clip(std::make_unique<TransformItem>(applyGeometryCallback ? window->windowItem()->geometryContainer() : window->windowItem()->effectContainer()))
    , m_snapshot(std::make_unique<SnapshotItem>(window, this, m_clip->childItems().front(), applyGeometryCallback))
{
    m_clip->setGlobalClipRect(RectF{});
    handleGeometryChanged();
    connect(m_clip.get(), &Item::boundingRectChanged, this, &OffscreenEffectItem::handleGeometryChanged);
    connect(m_clip.get(), &Item::childAdded, this, &OffscreenEffectItem::updateClip);
    connect(m_clip.get(), &Item::childRemoved, this, &OffscreenEffectItem::updateClip);
}

OffscreenEffectItem::~OffscreenEffectItem()
{
    disconnect(m_clip.get(), &Item::boundingRectChanged, this, nullptr);
}

void OffscreenEffectItem::setShader(GLShader *shader)
{
    m_snapshot->setShader(shader);
}

void OffscreenEffectItem::updateClip()
{
    if (m_clip->childItems().empty()) {
        return;
    }
    m_snapshot->setItem(m_clip->childItems().front());
    handleGeometryChanged();
}

void OffscreenEffectItem::handleGeometryChanged()
{
    m_snapshot->setGeometry(m_clip->boundingRect());
}

CrossfadeItem::CrossfadeItem(EffectWindow *window)
    : SnapshotItem(window, window->windowItem()->transformContainer(), window->windowItem()->transformContainer())
    , m_frameGeometryAtCapture(window->frameGeometry())
    , m_expandedGeometryAtCapture(window->expandedGeometry())
{
    freeze();
    updateGeometry();
}

CrossfadeItem::~CrossfadeItem()
{
}

void CrossfadeItem::updateGeometry()
{
    const RectF frameGeometry = m_window->frameGeometry();

    // This is for the case of *non* live effect, when the window buffer we saved has a different size
    // compared to the size the window has now. The "old" window will be rendered scaled to the current
    // window geometry, but everything will be scaled, also the shadow if there is any, making the window
    // frame not line up anymore with window->frameGeometry()
    // to fix that, we consider how much the shadow will have scaled, and use that as margins to the
    // current frame geometry. this causes the scaled window to visually line up perfectly with frameGeometry,
    // having the scaled shadow all outside of it.
    const qreal widthRatio = m_frameGeometryAtCapture.width() / frameGeometry.width();
    const qreal heightRatio = m_frameGeometryAtCapture.height() / frameGeometry.height();

    const QMarginsF margins{
        (m_expandedGeometryAtCapture.x() - m_frameGeometryAtCapture.x()) / widthRatio,
        (m_expandedGeometryAtCapture.y() - m_frameGeometryAtCapture.y()) / heightRatio,
        (m_frameGeometryAtCapture.right() - m_expandedGeometryAtCapture.right()) / widthRatio,
        (m_frameGeometryAtCapture.bottom() - m_expandedGeometryAtCapture.bottom()) / heightRatio,
    };

    setGeometry(RectF(QPointF(0, 0), frameGeometry.size()) - margins);
}

}

#include "moc_offscreeneffectitem.cpp"
