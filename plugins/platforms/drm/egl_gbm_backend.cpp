/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_backend.h"
// kwin
#include "composite.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "gbm_surface.h"
#include "logging.h"
#include "options.h"
#include "screens.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwineglimagetexture.h>
// system
#include <gbm.h>

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend)
    : AbstractEglBackend()
    , m_backend(drmBackend)
{
    // Egl is always direct rendering.
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    connect(m_backend, &DrmBackend::outputAdded, this, &EglGbmBackend::createOutput);
    connect(m_backend, &DrmBackend::outputRemoved, this, &EglGbmBackend::removeOutput);
}

EglGbmBackend::~EglGbmBackend()
{
    cleanup();
}

void EglGbmBackend::cleanupSurfaces()
{
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        cleanupOutput(*it);
    }
    m_outputs.clear();
}

void EglGbmBackend::cleanupFramebuffer(Output &output)
{
    if (!output.render.framebuffer) {
        return;
    }
    glDeleteTextures(1, &output.render.texture);
    output.render.texture = 0;
    glDeleteFramebuffers(1, &output.render.framebuffer);
    output.render.framebuffer = 0;
}

void EglGbmBackend::cleanupOutput(Output &output)
{
    cleanupFramebuffer(output);
    output.output->releaseGbm();

    if (output.eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay(), output.eglSurface);
    }
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        const bool hasMesaGBM = hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_gbm"));
        const bool hasKHRGBM = hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_gbm"));
        const GLenum platform = hasMesaGBM ? EGL_PLATFORM_GBM_MESA : EGL_PLATFORM_GBM_KHR;

        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) ||
                (!hasMesaGBM && !hasKHRGBM)) {
            setFailed("Missing one or more extensions between EGL_EXT_platform_base, "
                      "EGL_MESA_platform_gbm, EGL_KHR_platform_gbm");
            return false;
        }

        auto device = gbm_create_device(m_backend->fd());
        if (!device) {
            setFailed("Could not create gbm device");
            return false;
        }
        m_backend->setGbmDevice(device);

        display = eglGetPlatformDisplayEXT(platform, device, nullptr);
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void EglGbmBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initKWinGL();
    initBufferAge();
    initWayland();
}

bool EglGbmBackend::initRenderingContext()
{
    initBufferConfigs();
    if (!createContext()) {
        return false;
    }

    const auto outputs = m_backend->drmOutputs();

    for (DrmOutput *drmOutput: outputs) {
        createOutput(drmOutput);
    }

    if (m_outputs.isEmpty()) {
        qCCritical(KWIN_DRM) << "Create Window Surfaces failed";
        return false;
    }

    // Set our first surface as the one for the abstract backend, just to make it happy.
    setSurface(m_outputs.first().eglSurface);

    return makeContextCurrent(m_outputs.first());
}

std::shared_ptr<GbmSurface> EglGbmBackend::createGbmSurface(const QSize &size) const
{
    auto gbmSurface = std::make_shared<GbmSurface>(m_backend->gbmDevice(),
                                                   size.width(), size.height(),
                                                   GBM_FORMAT_XRGB8888,
                                                   GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gbmSurface) {
        qCCritical(KWIN_DRM) << "Creating GBM surface failed";
        return nullptr;
    }
    return gbmSurface;
}

EGLSurface EglGbmBackend::createEglSurface(std::shared_ptr<GbmSurface> gbmSurface) const
{
    auto eglSurface = eglCreatePlatformWindowSurfaceEXT(eglDisplay(), config(),
                                                        (void *)(gbmSurface->surface()), nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_DRM) << "Creating EGL surface failed";
        return EGL_NO_SURFACE;
    }
    return eglSurface;
}

bool EglGbmBackend::resetOutput(Output &output, DrmOutput *drmOutput)
{
    output.output = drmOutput;
    const QSize size = drmOutput->hardwareTransforms() ? drmOutput->pixelSize() :
                                                         drmOutput->modeSize();

    auto gbmSurface = createGbmSurface(size);
    if (!gbmSurface) {
        return false;
    }
    auto eglSurface = createEglSurface(gbmSurface);
    if (eglSurface == EGL_NO_SURFACE) {
        return false;
    }

    // destroy previous surface
    if (output.eglSurface != EGL_NO_SURFACE) {
        if (surface() == output.eglSurface) {
            setSurface(eglSurface);
        }
        eglDestroySurface(eglDisplay(), output.eglSurface);
    }
    output.eglSurface = eglSurface;
    output.gbmSurface = gbmSurface;

    resetFramebuffer(output);
    return true;
}

void EglGbmBackend::createOutput(DrmOutput *drmOutput)
{
    Output newOutput;
    if (resetOutput(newOutput, drmOutput)) {
        connect(drmOutput, &DrmOutput::modeChanged, this,
            [drmOutput, this] {
                auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                    [drmOutput] (const auto &output) {
                        return output.output == drmOutput;
                    }
                );
                if (it == m_outputs.end()) {
                    return;
                }
                resetOutput(*it, drmOutput);
            }
        );
        m_outputs << newOutput;
    }
}

void EglGbmBackend::removeOutput(DrmOutput *drmOutput)
{
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
        [drmOutput] (const Output &output) {
            return output.output == drmOutput;
        }
    );
    if (it == m_outputs.end()) {
        return;
    }

    cleanupOutput(*it);
    m_outputs.erase(it);
}

const float vertices[] = {
   -1.0f,  1.0f,
   -1.0f, -1.0f,
    1.0f, -1.0f,

   -1.0f,  1.0f,
    1.0f, -1.0f,
    1.0f,  1.0f,
};

const float texCoords[] = {
    0.0f,  1.0f,
    0.0f,  0.0f,
    1.0f,  0.0f,

    0.0f,  1.0f,
    1.0f,  0.0f,
    1.0f,  1.0f
};

bool EglGbmBackend::resetFramebuffer(Output &output)
{
    cleanupFramebuffer(output);

    if (output.output->hardwareTransforms()) {
        // No need for an extra render target.
        return true;
    }

    makeContextCurrent(output);

    glGenFramebuffers(1, &output.render.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, output.render.framebuffer);
    GLRenderTarget::setKWinFramebuffer(output.render.framebuffer);

    glGenTextures(1, &output.render.texture);
    glBindTexture(GL_TEXTURE_2D, output.render.texture);

    const QSize texSize = output.output->pixelSize();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize.width(), texSize.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           output.render.texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        qCWarning(KWIN_DRM) << "Error: framebuffer not complete";
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLRenderTarget::setKWinFramebuffer(0);
    return true;
}

void EglGbmBackend::initRenderTarget(Output &output)
{
    if (output.render.vbo) {
        // Already initialized.
        return;
    }
    std::shared_ptr<GLVertexBuffer> vbo(new GLVertexBuffer(KWin::GLVertexBuffer::Static));
    vbo->setData(6, 2, vertices, texCoords);
    output.render.vbo = vbo;
}

void EglGbmBackend::renderFramebufferToSurface(Output &output)
{
    if (!output.render.framebuffer) {
        // No additional render target.
        return;
    }
    initRenderTarget(output);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLRenderTarget::setKWinFramebuffer(0);

    const auto size = output.output->modeSize();
    glViewport(0, 0, size.width(), size.height());

    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 mvpMatrix;

    const DrmOutput *drmOutput = output.output;
    switch (drmOutput->transform()) {
    case DrmOutput::Transform::Normal:
    case DrmOutput::Transform::Flipped:
        break;
    case DrmOutput::Transform::Rotated90:
    case DrmOutput::Transform::Flipped90:
        mvpMatrix.rotate(90, 0, 0, 1);
        break;
    case DrmOutput::Transform::Rotated180:
    case DrmOutput::Transform::Flipped180:
        mvpMatrix.rotate(180, 0, 0, 1);
        break;
    case DrmOutput::Transform::Rotated270:
    case DrmOutput::Transform::Flipped270:
        mvpMatrix.rotate(270, 0, 0, 1);
        break;
    }
    switch (drmOutput->transform()) {
    case DrmOutput::Transform::Flipped:
    case DrmOutput::Transform::Flipped90:
    case DrmOutput::Transform::Flipped180:
    case DrmOutput::Transform::Flipped270:
        mvpMatrix.scale(-1, 1);
        break;
    default:
        break;
    }

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvpMatrix);

    glBindTexture(GL_TEXTURE_2D, output.render.texture);
    output.render.vbo->render(GL_TRIANGLES);
    ShaderManager::instance()->popShader();
}

void EglGbmBackend::prepareRenderFramebuffer(const Output &output) const
{
    // When render.framebuffer is 0 we may just reset to the screen framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, output.render.framebuffer);
    GLRenderTarget::setKWinFramebuffer(output.render.framebuffer);
}

bool EglGbmBackend::makeContextCurrent(const Output &output) const
{
    const EGLSurface surface = output.eglSurface;
    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    if (eglMakeCurrent(eglDisplay(), surface, surface, context()) == EGL_FALSE) {
        qCCritical(KWIN_DRM) << "Make Context Current failed";
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_DRM) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

bool EglGbmBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (!eglChooseConfig(eglDisplay(), config_attribs, configs,
                         sizeof(configs) / sizeof(EGLConfig),
                         &count)) {
        qCCritical(KWIN_DRM) << "choose config failed";
        return false;
    }

    qCDebug(KWIN_DRM) << "EGL buffer configs count:" << count;

    // Loop through all configs, choosing the first one that has suitable format.
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat;
        // Query some configuration parameters, to show in debug log.
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);

        if (KWIN_DRM().isDebugEnabled()) {
            // GBM formats are declared as FOURCC code (integer from ASCII chars, so use this fact).
            char gbmFormatStr[sizeof(EGLint) + 1] = {0};
            memcpy(gbmFormatStr, &gbmFormat, sizeof(EGLint));

            // Query number of bits for color channel.
            EGLint blueSize, redSize, greenSize, alphaSize;
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &redSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &greenSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blueSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &alphaSize);
            qCDebug(KWIN_DRM) << "  EGL config #" << i << " has GBM FOURCC format:" << gbmFormatStr
                              << "; color sizes (RGBA order):"
                              << redSize << greenSize << blueSize << alphaSize;
        }

        if ((gbmFormat == GBM_FORMAT_XRGB8888) || (gbmFormat == GBM_FORMAT_ARGB8888)) {
            setConfig(configs[i]);
            return true;
        }
    }

    qCCritical(KWIN_DRM) << "Choosing EGL config did not return a suitable config. There were"
                         << count << "configs.";
    return false;
}

void EglGbmBackend::present()
{
    Q_UNREACHABLE();
    // Not in use. This backend does per-screen rendering.
}

static QVector<EGLint> regionToRects(const QRegion &region, AbstractWaylandOutput *output)
{
    const int height = output->modeSize().height();

    const QMatrix4x4 matrix = DrmOutput::logicalToNativeMatrix(output->geometry(),
                                                               output->scale(),
                                                               output->transform());

    QVector<EGLint> rects;
    rects.reserve(region.rectCount() * 4);
    for (const QRect &_rect : region) {
        const QRect rect = matrix.mapRect(_rect);

        rects << rect.left();
        rects << height - (rect.y() + rect.height());
        rects << rect.width();
        rects << rect.height();
    }
    return rects;
}

void EglGbmBackend::aboutToStartPainting(const QRegion &damagedRegion)
{
    // See EglGbmBackend::endRenderingFrameForScreen comment for the reason why we only support screenId=0
    if (m_outputs.count() > 1) {
        return;
    }

    const Output &output = m_outputs.at(0);
    if (output.bufferAge > 0 && !damagedRegion.isEmpty() && supportsPartialUpdate()) {
        const QRegion region = damagedRegion & output.output->geometry();

        QVector<EGLint> rects = regionToRects(region, output.output);
        const bool correct = eglSetDamageRegionKHR(eglDisplay(), output.eglSurface,
                                                   rects.data(), rects.count()/4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "failed eglSetDamageRegionKHR" << eglGetError();
        }
    }
}

void EglGbmBackend::presentOnOutput(Output &output, const QRegion &damagedRegion)
{
    if (supportsSwapBuffersWithDamage()) {
        QVector<EGLint> rects = regionToRects(output.damageHistory.constFirst(), output.output);
        eglSwapBuffersWithDamageEXT(eglDisplay(), output.eglSurface,
                                    rects.data(), rects.count()/4);
    } else {
        eglSwapBuffers(eglDisplay(), output.eglSurface);
    }
    output.buffer = m_backend->createBuffer(output.gbmSurface);

    Q_EMIT output.output->outputChange(damagedRegion);
    m_backend->present(output.buffer, output.output);

    if (supportsBufferAge()) {
        eglQuerySurface(eglDisplay(), output.eglSurface, EGL_BUFFER_AGE_EXT, &output.bufferAge);
    }
}

void EglGbmBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // TODO, create new buffer?
}

SceneOpenGLTexturePrivate *EglGbmBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglGbmTexture(texture, this);
}

QRegion EglGbmBackend::prepareRenderingFrame()
{
    startRenderTimer();
    return QRegion();
}

void EglGbmBackend::setViewport(const Output &output) const
{
    const QSize &overall = screens()->size();
    const QRect &v = output.output->geometry();
    qreal scale = output.output->scale();

    glViewport(-v.x() * scale, (v.height() - overall.height() + v.y()) * scale,
               overall.width() * scale, overall.height() * scale);
}

QRegion EglGbmBackend::prepareRenderingForScreen(int screenId)
{
    const Output &output = m_outputs.at(screenId);

    makeContextCurrent(output);
    prepareRenderFramebuffer(output);
    setViewport(output);

    if (supportsBufferAge()) {
        QRegion region;

        // Note: An age of zero means the buffer contents are undefined
        if (output.bufferAge > 0 && output.bufferAge <= output.damageHistory.count()) {
            for (int i = 0; i < output.bufferAge - 1; i++)
                region |= output.damageHistory[i];
        } else {
            region = output.output->geometry();
        }

        return region;
    }
    return output.output->geometry();
}

void EglGbmBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)
}

void EglGbmBackend::endRenderingFrameForScreen(int screenId,
                                               const QRegion &renderedRegion,
                                               const QRegion &damagedRegion)
{
    Output &output = m_outputs[screenId];
    renderFramebufferToSurface(output);

    if (damagedRegion.intersected(output.output->geometry()).isEmpty() && screenId == 0) {

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.intersected(output.output->geometry()).isEmpty())
            glFlush();

        for (auto &output: m_outputs) {
            output.bufferAge = 1;
        }
        return;
    }
    presentOnOutput(output, damagedRegion);

    // Save the damaged region to history
    // Note: damage history is only collected for the first screen. For any other screen full
    // repaints are triggered. This is due to a limitation in Scene::paintGenericScreen which resets
    // the Toplevel's repaint. So multiple calls to Scene::paintScreen as it's done in multi-output
    // rendering only have correct damage information for the first screen. If we try to track
    // damage nevertheless, it creates artifacts. So for the time being we work around the problem
    // by only supporting buffer age on the first output. To properly support buffer age on all
    // outputs the rendering needs to be refactored in general.
    if (supportsBufferAge() && screenId == 0) {
        if (output.damageHistory.count() > 10) {
            output.damageHistory.removeLast();
        }
        output.damageHistory.prepend(damagedRegion.intersected(output.output->geometry()));
    }
}

bool EglGbmBackend::usesOverlayWindow() const
{
    return false;
}

bool EglGbmBackend::perScreenRendering() const
{
    return true;
}

QSharedPointer<GLTexture> EglGbmBackend::textureForOutput(AbstractOutput *abstractOutput) const
{
    const QVector<KWin::EglGbmBackend::Output>::const_iterator itOutput = std::find_if(m_outputs.begin(), m_outputs.end(),
        [abstractOutput] (const auto &output) {
            return output.output == abstractOutput;
        }
    );
    if (itOutput == m_outputs.end()) {
        return {};
    }

    DrmOutput *drmOutput = itOutput->output;
    if (!drmOutput->hardwareTransforms()) {
        const auto glTexture = QSharedPointer<KWin::GLTexture>::create(itOutput->render.texture, GL_RGBA8, drmOutput->pixelSize());
        glTexture->setYInverted(true);
        return glTexture;
    }

    EGLImageKHR image = eglCreateImageKHR(eglDisplay(), nullptr, EGL_NATIVE_PIXMAP_KHR, itOutput->buffer->getBo(), nullptr);
    if (image == EGL_NO_IMAGE_KHR) {
        qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << glGetError();
        return {};
    }

    return QSharedPointer<EGLImageTexture>::create(eglDisplay(), image, GL_RGBA8, drmOutput->modeSize());
}

/************************************************
 * EglTexture
 ************************************************/

EglGbmTexture::EglGbmTexture(KWin::SceneOpenGLTexture *texture, EglGbmBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglGbmTexture::~EglGbmTexture() = default;

}
