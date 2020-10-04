/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_stream_backend.h"
#include "composite.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "logging.h"
#include "logind.h"
#include "options.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include <kwinglplatform.h>
#include "drm_gpu.h"

namespace KWin
{

typedef EGLStreamKHR (*PFNEGLCREATESTREAMATTRIBNV)(EGLDisplay, EGLAttrib *);
typedef EGLBoolean (*PFNEGLGETOUTPUTLAYERSEXT)(EGLDisplay, EGLAttrib *, EGLOutputLayerEXT *, EGLint, EGLint *);
typedef EGLBoolean (*PFNEGLSTREAMCONSUMEROUTPUTEXT)(EGLDisplay, EGLStreamKHR, EGLOutputLayerEXT);
typedef EGLSurface (*PFNEGLCREATESTREAMPRODUCERSURFACEKHR)(EGLDisplay, EGLConfig, EGLStreamKHR, EGLint *);
typedef EGLBoolean (*PFNEGLDESTROYSTREAMKHR)(EGLDisplay, EGLStreamKHR);
typedef EGLBoolean (*PFNEGLSTREAMCONSUMERACQUIREATTRIBNV)(EGLDisplay, EGLStreamKHR, EGLAttrib *);
PFNEGLCREATESTREAMATTRIBNV pEglCreateStreamAttribNV = nullptr;
PFNEGLGETOUTPUTLAYERSEXT pEglGetOutputLayersEXT = nullptr;
PFNEGLSTREAMCONSUMEROUTPUTEXT pEglStreamConsumerOutputEXT = nullptr;
PFNEGLCREATESTREAMPRODUCERSURFACEKHR pEglCreateStreamProducerSurfaceKHR = nullptr;
PFNEGLDESTROYSTREAMKHR pEglDestroyStreamKHR = nullptr;
PFNEGLSTREAMCONSUMERACQUIREATTRIBNV pEglStreamConsumerAcquireAttribNV = nullptr;

#ifndef EGL_CONSUMER_AUTO_ACQUIRE_EXT
#define EGL_CONSUMER_AUTO_ACQUIRE_EXT 0x332B
#endif

#ifndef EGL_DRM_MASTER_FD_EXT
#define EGL_DRM_MASTER_FD_EXT 0x333C
#endif

#ifndef EGL_DRM_FLIP_EVENT_DATA_NV
#define EGL_DRM_FLIP_EVENT_DATA_NV 0x333E
#endif

EglStreamBackend::EglStreamBackend(DrmBackend *drmBackend, DrmGpu *gpu)
    : AbstractEglDrmBackend(drmBackend, gpu)
{
}

void EglStreamBackend::cleanupSurfaces()
{
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        cleanupOutput(*it);
    }
    m_outputs.clear();
}

void EglStreamBackend::cleanupOutput(const Output &o)
{
    if (o.buffer != nullptr) {
        delete o.buffer;
    }
    if (o.eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay(), o.eglSurface);
    }
    if (o.eglStream != EGL_NO_STREAM_KHR) {
        pEglDestroyStreamKHR(eglDisplay(), o.eglStream);
    }
}

bool EglStreamBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_gpu->eglDisplay();
    if (display == EGL_NO_DISPLAY) {
        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_device_base")) &&
            !(hasClientExtension(QByteArrayLiteral("EGL_EXT_device_query")) &&
              hasClientExtension(QByteArrayLiteral("EGL_EXT_device_enumeration")))) {
            setFailed("Missing required EGL client extension: "
                      "EGL_EXT_device_base or "
                      "EGL_EXT_device_query and EGL_EXT_device_enumeration");
            return false;
        }

        // Try to find the EGLDevice corresponding to our DRM device file
        int numDevices;
        eglQueryDevicesEXT(0, nullptr, &numDevices);
        QVector<EGLDeviceEXT> devices(numDevices);
        eglQueryDevicesEXT(numDevices, devices.data(), &numDevices);
        for (EGLDeviceEXT device : devices) {
            const char *drmDeviceFile = eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);
            if (m_gpu->devNode().compare(drmDeviceFile)) {
                continue;
            }

            const char *deviceExtensionCString = eglQueryDeviceStringEXT(device, EGL_EXTENSIONS);
            QByteArray deviceExtensions = QByteArray::fromRawData(deviceExtensionCString,
                                                                  qstrlen(deviceExtensionCString));
            if (!deviceExtensions.split(' ').contains(QByteArrayLiteral("EGL_EXT_device_drm"))) {
                continue;
            }

            EGLint platformAttribs[] = {
                EGL_DRM_MASTER_FD_EXT, m_gpu->fd(),
                EGL_NONE
            };
            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, device, platformAttribs);
            break;
        }
        m_gpu->setEglDisplay(display);
    }

    if (display == EGL_NO_DISPLAY) {
        setFailed("No suitable EGL device found");
        return false;
    }

    setEglDisplay(display);
    if (!initEglAPI()) {
        return false;
    }

    const QVector<QByteArray> requiredExtensions = {
        QByteArrayLiteral("EGL_EXT_output_base"),
        QByteArrayLiteral("EGL_EXT_output_drm"),
        QByteArrayLiteral("EGL_KHR_stream"),
        QByteArrayLiteral("EGL_KHR_stream_producer_eglsurface"),
        QByteArrayLiteral("EGL_EXT_stream_consumer_egloutput"),
        QByteArrayLiteral("EGL_NV_stream_attrib"),
        QByteArrayLiteral("EGL_EXT_stream_acquire_mode"),
    };
    for (const QByteArray &ext : requiredExtensions) {
        if (!hasExtension(ext)) {
            setFailed(QStringLiteral("Missing required EGL extension: ") + ext);
            return false;
        }
    }

    pEglCreateStreamAttribNV = (PFNEGLCREATESTREAMATTRIBNV)eglGetProcAddress("eglCreateStreamAttribNV");
    pEglGetOutputLayersEXT = (PFNEGLGETOUTPUTLAYERSEXT)eglGetProcAddress("eglGetOutputLayersEXT");
    pEglStreamConsumerOutputEXT = (PFNEGLSTREAMCONSUMEROUTPUTEXT)eglGetProcAddress("eglStreamConsumerOutputEXT");
    pEglCreateStreamProducerSurfaceKHR = (PFNEGLCREATESTREAMPRODUCERSURFACEKHR)eglGetProcAddress("eglCreateStreamProducerSurfaceKHR");
    pEglDestroyStreamKHR = (PFNEGLDESTROYSTREAMKHR)eglGetProcAddress("eglDestroyStreamKHR");
    pEglStreamConsumerAcquireAttribNV = (PFNEGLSTREAMCONSUMERACQUIREATTRIBNV)eglGetProcAddress("eglStreamConsumerAcquireAttribNV");
    return true;
}

void EglStreamBackend::init()
{
    if (!m_gpu->atomicModeSetting()) {
        setFailed("EGLStream backend requires atomic modesetting");
        return;
    }

    if (!initializeEgl()) {
        setFailed("Failed to initialize EGL api");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Failed to initialize rendering context");
        return;
    }

    initKWinGL();
    setSupportsBufferAge(false);
    initWayland();
}

bool EglStreamBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    const auto outputs = m_gpu->outputs();
    for (DrmOutput *drmOutput : outputs) {
        addOutput(drmOutput);
    }
    if (m_outputs.isEmpty()) {
        qCCritical(KWIN_DRM) << "Failed to create output surface";
        return false;
    }
    // set our first surface as the one for the abstract backend
    setSurface(m_outputs.first().eglSurface);

    return makeContextCurrent(m_outputs.first());
}

bool EglStreamBackend::resetOutput(Output &o, DrmOutput *drmOutput)
{
    o.output = drmOutput;
    if (o.buffer != nullptr) {
        delete o.buffer;
    }
    // dumb buffer used for modesetting
    o.buffer = m_gpu->createBuffer(drmOutput->pixelSize());

    EGLAttrib streamAttribs[] = {
        EGL_STREAM_FIFO_LENGTH_KHR, 0, // mailbox mode
        EGL_CONSUMER_AUTO_ACQUIRE_EXT, EGL_FALSE,
        EGL_NONE
    };
    EGLStreamKHR stream = pEglCreateStreamAttribNV(eglDisplay(), streamAttribs);
    if (stream == EGL_NO_STREAM_KHR) {
        qCCritical(KWIN_DRM) << "Failed to create EGL stream for output";
        return false;
    }

    EGLAttrib outputAttribs[3];
    if (drmOutput->primaryPlane()) {
        outputAttribs[0] = EGL_DRM_PLANE_EXT;
        outputAttribs[1] = drmOutput->primaryPlane()->id();
    } else {
        outputAttribs[0] = EGL_DRM_CRTC_EXT;
        outputAttribs[1] = drmOutput->crtc()->id();
    }
    outputAttribs[2] = EGL_NONE;
    EGLint numLayers;
    EGLOutputLayerEXT outputLayer;
    pEglGetOutputLayersEXT(eglDisplay(), outputAttribs, &outputLayer, 1, &numLayers);
    if (numLayers == 0) {
        qCCritical(KWIN_DRM) << "No EGL output layers found";
        return false;
    }

    pEglStreamConsumerOutputEXT(eglDisplay(), stream, outputLayer);
    EGLint streamProducerAttribs[] = {
        EGL_WIDTH, drmOutput->pixelSize().width(),
        EGL_HEIGHT, drmOutput->pixelSize().height(),
        EGL_NONE
    };
    EGLSurface eglSurface = pEglCreateStreamProducerSurfaceKHR(eglDisplay(), config(), stream,
                                                               streamProducerAttribs);
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_DRM) << "Failed to create EGL surface for output";
        return false;
    }

    if (o.eglSurface != EGL_NO_SURFACE) {
        if (surface() == o.eglSurface) {
            setSurface(eglSurface);
        }
        eglDestroySurface(eglDisplay(), o.eglSurface);
    }

    if (o.eglStream != EGL_NO_STREAM_KHR) {
        pEglDestroyStreamKHR(eglDisplay(), o.eglStream);
    }

    o.eglStream = stream;
    o.eglSurface = eglSurface;
    return true;
}

void EglStreamBackend::addOutput(DrmOutput *drmOutput)
{
    Q_ASSERT(drmOutput->gpu() == m_gpu);
    Output o;
    if (!resetOutput(o, drmOutput)) {
        return;
    }

    connect(drmOutput, &DrmOutput::modeChanged, this,
        [drmOutput, this] {
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [drmOutput] (const auto &o) {
                    return o.output == drmOutput;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            resetOutput(*it, drmOutput);
        }
    );
    m_outputs << o;
}

void EglStreamBackend::removeOutput(DrmOutput *drmOutput)
{
    Q_ASSERT(drmOutput->gpu() == m_gpu);
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
        [drmOutput] (const Output &o) {
            return o.output == drmOutput;
        }
    );
    if (it == m_outputs.end()) {
        return;
    }
    cleanupOutput(*it);
    m_outputs.erase(it);
}

bool EglStreamBackend::makeContextCurrent(const Output &output)
{
    const EGLSurface surface = output.eglSurface;
    if (surface == EGL_NO_SURFACE) {
        return false;
    }

    if (eglMakeCurrent(eglDisplay(), surface, surface, context()) == EGL_FALSE) {
        qCCritical(KWIN_DRM) << "Failed to make EGL context current";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_DRM) << "Error occurred while making EGL context current" << error;
        return false;
    }

    const QSize &overall = screens()->size();
    const QRect &v = output.output->geometry();
    qreal scale = output.output->scale();
    glViewport(-v.x() * scale, (v.height() - overall.height() + v.y()) * scale,
               overall.width() * scale, overall.height() * scale);
    return true;
}

bool EglStreamBackend::initBufferConfigs()
{
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE,         EGL_STREAM_BIT_KHR,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };
    EGLint count;
    EGLConfig config;
    if (!eglChooseConfig(eglDisplay(), configAttribs, &config, 1, &count)) {
        qCCritical(KWIN_DRM) << "Failed to query available EGL configs";
        return false;
    }
    if (count == 0) {
        qCCritical(KWIN_DRM) << "No suitable EGL config found";
        return false;
    }

    setConfig(config);
    return true;
}

bool EglStreamBackend::presentOnOutput(EglStreamBackend::Output &o)
{
    if (!eglSwapBuffers(eglDisplay(), o.eglSurface)) {
        qCCritical(KWIN_DRM, "eglSwapBuffers() failed: %x", eglGetError());
        return false;
    }

    return m_backend->present(o.buffer, o.output);
}

SceneOpenGLTexturePrivate *EglStreamBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglStreamTexture(texture, this);
}

QRegion EglStreamBackend::beginFrame(int screenId)
{
    const Output &o = m_outputs.at(screenId);
    makeContextCurrent(o);
    return o.output->geometry();
}

void EglStreamBackend::endFrame(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion);
    Q_UNUSED(damagedRegion);

    Output &renderOutput = m_outputs[screenId];
    DrmOutput *drmOutput = renderOutput.output;

    if (!presentOnOutput(renderOutput)) {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(drmOutput->renderLoop());
        renderLoopPrivate->notifyFrameFailed();
        return;
    }

    EGLAttrib acquireAttribs[] = {
        EGL_DRM_FLIP_EVENT_DATA_NV, (EGLAttrib)drmOutput,
        EGL_NONE,
    };
    if (!pEglStreamConsumerAcquireAttribNV(eglDisplay(), renderOutput.eglStream, acquireAttribs)) {
        qCWarning(KWIN_DRM) << "Failed to acquire output EGL stream frame";
    }
}

/************************************************
 * EglTexture
 ************************************************/

EglStreamTexture::EglStreamTexture(SceneOpenGLTexture *texture, EglStreamBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglStreamTexture::~EglStreamTexture()
{
}

} // namespace
