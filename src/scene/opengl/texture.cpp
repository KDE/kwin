/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/opengl/texture.h"
#include "compositor.h"
#include "core/drm_formats.h"
#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbufferview.h"
#include "core/renderdevice.h"
#include "core/syncobjtimeline.h"
#include "multigpuswapchain.h"
#include "opengl/eglbackend.h"
#include "opengl/eglnativefence.h"
#include "opengl/gltexture.h"
#include "utils/common.h"

#include <QSocketNotifier>

namespace KWin
{

TextureOpenGL::TextureOpenGL(const std::shared_ptr<EglContext> &context)
    : m_context(context)
{
}

TextureOpenGL::~TextureOpenGL()
{
    (void)m_context->makeCurrent();
    m_texture.reset();
}

GLTexture *TextureOpenGL::texture() const
{
    return m_texture.get();
}

ImageTextureOpenGL::ImageTextureOpenGL(const std::shared_ptr<EglContext> &context)
    : TextureOpenGL(context)
{
}

std::unique_ptr<ImageTextureOpenGL> ImageTextureOpenGL::create(const std::shared_ptr<EglContext> &context, const QImage &image)
{
    auto texture = std::make_unique<ImageTextureOpenGL>(context);
    if (texture->upload(image)) {
        return texture;
    }

    return nullptr;
}

void ImageTextureOpenGL::attach(GraphicsBuffer *buffer, const Region &region,
                                const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                const std::shared_ptr<ColorDescription> &color)
{
    Q_UNREACHABLE();
}

bool ImageTextureOpenGL::upload(const QImage &image)
{
    auto nativeTexture = GLTexture::upload(image);
    if (!nativeTexture) {
        return false;
    }

    nativeTexture->setFilter(GL_LINEAR);
    nativeTexture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_texture = std::move(nativeTexture);
    m_size = m_texture->size();

    return true;
}

void ImageTextureOpenGL::upload(const QImage &image, const Rect &rect)
{
    m_texture->update(image, rect);
}

BufferTextureOpenGL::BufferTextureOpenGL(RenderDevice *renderDevice)
    : TextureOpenGL(renderDevice->eglContext())
    , m_renderDevice(renderDevice)
{
}

BufferTextureOpenGL::~BufferTextureOpenGL()
{
}

std::unique_ptr<BufferTextureOpenGL> BufferTextureOpenGL::create(RenderDevice *device, GraphicsBuffer *buffer,
                                                                 const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                                                 const std::shared_ptr<ColorDescription> &color)
{
    auto texture = std::make_unique<BufferTextureOpenGL>(device);
    if (texture->attach(buffer, releasePoint, color)) {
        return texture;
    }

    return nullptr;
}

bool BufferTextureOpenGL::attach(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint, const std::shared_ptr<ColorDescription> &color)
{
    if (buffer->dmabufAttributes()) {
        return loadDmabufTexture(buffer, releasePoint, color);
    } else if (buffer->shmAttributes()) {
        if (EGLImageKHR image = m_renderDevice->eglDisplay()->importBufferAsImage(buffer, color->yuvCoefficients(), color->range())) {
            return loadUDmabufTexture(buffer, image);
        } else {
            return loadShmTexture(buffer);
        }
    } else if (buffer->singlePixelAttributes()) {
        return loadSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to create OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
        return false;
    }
}

void BufferTextureOpenGL::attach(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                 const std::shared_ptr<ColorDescription> &color)
{
    if (buffer->dmabufAttributes()) {
        updateDmabufTexture(buffer, region, releasePoint, color);
    } else if (buffer->shmAttributes()) {
        if (EGLImageKHR image = m_renderDevice->eglDisplay()->importBufferAsImage(buffer, color->yuvCoefficients(), color->range())) {
            updateUDmabufTexture(buffer, image, releasePoint);
        } else {
            updateShmTexture(buffer, region);
        }
    } else if (buffer->singlePixelAttributes()) {
        updateSinglePixelTexture(buffer, releasePoint);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to update OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
    }
}

void BufferTextureOpenGL::upload(const QImage &image, const Rect &rect)
{
    Q_UNREACHABLE();
}

void BufferTextureOpenGL::reset()
{
    m_texture.reset();
    m_bufferType = BufferType::None;
    m_size = QSize();
    m_releasePoint.reset();
}

static std::unique_ptr<GLTexture> createTexture(EGLImageKHR image, const QSize &size, bool isExternalOnly)
{
    if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        return nullptr;
    }

    GLint target = isExternalOnly ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
    auto texture = std::make_unique<GLTexture>(target);
    texture->setSize(size);
    if (!texture->create()) {
        return nullptr;
    }
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setFilter(GL_LINEAR);
    texture->bind();
    glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(image));
    texture->unbind();
    texture->setContentTransform(OutputTransform::FlipY);
    return texture;
}

bool BufferTextureOpenGL::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return false;
    }

    std::unique_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setContentTransform(OutputTransform::FlipY);

    m_bufferType = BufferType::Shm;
    m_texture = std::move(texture);
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;

    return true;
}

static Region simplifyDamage(const Region &damage)
{
    if (damage.rects().size() < 3) {
        return damage;
    } else {
        return damage.boundingRect();
    }
}

void BufferTextureOpenGL::updateShmTexture(GraphicsBuffer *buffer, const Region &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        reset();
        loadShmTexture(buffer);
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    m_texture->update(*view.image(), simplifyDamage(region) & Rect(QPoint(0, 0), m_texture->size()));
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
}

bool BufferTextureOpenGL::loadDmabufTexture(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                            const std::shared_ptr<ColorDescription> &color)
{
    auto attribs = buffer->dmabufAttributes();
    m_dmabufDevice = attribs->device;
    RenderDevice *compat = GpuManager::self()->compatibleRenderDevice(attribs->device);
    if (!compat) {
        qCCritical(KWIN_OPENGL, "Couldn't find a compatible GPU for a buffer");
        return false;
    } else if (compat == m_renderDevice) {
        m_mgpuSwapchain.reset();
        m_releasePoint = releasePoint;
    } else {
        // need to do a multi gpu copy
        m_mgpuSwapchain = MultiGpuSwapchain::createForSampling(compat, m_renderDevice,
                                                               attribs->format, attribs->modifier, buffer->size(),
                                                               m_renderDevice->eglDisplay()->allSupportedDrmFormats());
        if (!m_mgpuSwapchain) {
            qCCritical(KWIN_OPENGL, "Couldn't create multi gpu swapchain for a buffer %s 0x%lx", qPrintable(FormatInfo::drmFormatName(attribs->format)), attribs->modifier);
            return false;
        }
        EGLNativeFence releaseFence(m_renderDevice->eglDisplay());
        auto imported = m_mgpuSwapchain->copyRgbBuffer(buffer, Region::infinite(), releaseFence.takeFileDescriptor(),
                                                       nullptr, releasePoint);
        if (!imported.has_value()) {
            return false;
        }
        const auto fence = EGLNativeFence::importFence(m_renderDevice->eglDisplay(), std::move(imported->sync));
        if (!fence.waitSync()) {
            return false;
        }
        buffer = imported->buffer;
        attribs = buffer->dmabufAttributes();
        m_releasePoint = imported->releasePoint;
    }

    const bool isExternal = m_renderDevice->eglDisplay()->isExternalOnly(attribs->format, attribs->modifier);
    auto texture = createTexture(m_renderDevice->eglDisplay()->importBufferAsImage(buffer, color->yuvCoefficients(), color->range()), buffer->size(), isExternal);
    if (!texture) {
        return false;
    }
    m_texture = std::move(texture);

    m_bufferType = BufferType::DmaBuf;
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;

    return true;
}

void BufferTextureOpenGL::updateDmabufTexture(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint,
                                              const std::shared_ptr<ColorDescription> &color)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)
        || Q_UNLIKELY(m_dmabufDevice != buffer->dmabufAttributes()->device)
        || (m_mgpuSwapchain && !m_mgpuSwapchain->isSuitableFor(buffer))) {
        reset();
        attach(buffer, releasePoint, color);
        return;
    }
    if (m_mgpuSwapchain) {
        EGLNativeFence releaseFence(m_renderDevice->eglDisplay());
        auto imported = m_mgpuSwapchain->copyRgbBuffer(buffer, region, releaseFence.takeFileDescriptor(),
                                                       nullptr, releasePoint);
        if (!imported.has_value()) {
            return;
        }
        const auto fence = EGLNativeFence::importFence(m_renderDevice->eglDisplay(), std::move(imported->sync));
        if (!fence.waitSync()) {
            return;
        }
        buffer = imported->buffer;
        m_releasePoint = imported->releasePoint;
    } else {
        m_releasePoint = releasePoint;
    }

    const EGLImage image = m_renderDevice->eglDisplay()->importBufferAsImage(buffer, color->yuvCoefficients(), color->range());
    m_texture->bind();
    glEGLImageTargetTexture2DOES(m_texture->target(), static_cast<GLeglImageOES>(image));
    m_texture->unbind();

    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
}

bool BufferTextureOpenGL::loadSinglePixelTexture(GraphicsBuffer *buffer)
{
    // TODO this shouldn't allocate a texture,
    // the renderer should just use a color in the shader
    const GraphicsBufferView view(buffer);
    std::unique_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }
    m_texture = std::move(texture);
    m_bufferType = BufferType::SinglePixel;
    m_size = QSize(1, 1);
    m_isFloatingPoint = false;
    return true;
}

void BufferTextureOpenGL::updateSinglePixelTexture(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::SinglePixel)) {
        reset();
        loadSinglePixelTexture(buffer);
        return;
    }
    const GraphicsBufferView view(buffer);
    m_texture->update(*view.image(), Rect(0, 0, 1, 1));
}

class UDmabufReleasePoint : public SyncReleasePoint
{
public:
    explicit UDmabufReleasePoint(GraphicsBuffer *shmBuffer)
        : m_buffer(shmBuffer)
    {
    }

    ~UDmabufReleasePoint()
    {
        if (m_sync) {
            new SyncReferencer(m_buffer.get(), std::move(m_sync));
        }
    }

    void addReleaseFence(const FileDescriptor &fd) override
    {
        if (m_sync) {
            m_sync = mergeSyncFds(m_sync, fd);
        } else {
            m_sync = fd.duplicate();
        }
    }

    GraphicsBufferRef m_buffer;
    FileDescriptor m_sync;
};

bool BufferTextureOpenGL::loadUDmabufTexture(GraphicsBuffer *buffer, EGLImageKHR image)
{
    const bool isExternal = m_renderDevice->eglDisplay()->isExternalOnly(buffer->shmAttributes()->format, DRM_FORMAT_MOD_LINEAR);
    auto texture = createTexture(image, buffer->size(), isExternal);
    if (!texture) {
        return false;
    }
    m_texture = std::move(texture);
    m_bufferType = BufferType::UDmaBuf;
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
    m_releasePoint = std::make_shared<UDmabufReleasePoint>(buffer);
    return true;
}

void BufferTextureOpenGL::updateUDmabufTexture(GraphicsBuffer *buffer, EGLImageKHR image, const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::UDmaBuf)) {
        reset();
        loadUDmabufTexture(buffer, image);
        return;
    }
    const bool isExternal = m_renderDevice->eglDisplay()->isExternalOnly(buffer->shmAttributes()->format, DRM_FORMAT_MOD_LINEAR);
    const GLint target = isExternal ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;

    m_texture->bind();
    glEGLImageTargetTexture2DOES(target, image);
    m_texture->unbind();

    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
    m_releasePoint = std::make_shared<UDmabufReleasePoint>(buffer);
}

} // namespace KWin
