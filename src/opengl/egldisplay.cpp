/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egldisplay.h"
#include "core/drmdevice.h"
#include "core/graphicsbuffer.h"
#include "opengl/eglutils_p.h"
#include "opengl/glutils.h"
#include "utils/common.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>
#include <utils/drm_format_helper.h>

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

namespace KWin
{

bool EglDisplay::shouldUseOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

std::unique_ptr<EglDisplay> EglDisplay::create(::EGLDisplay display, bool owning)
{
    if (!display) {
        return nullptr;
    }
    EGLint major, minor;
    if (eglInitialize(display, &major, &minor) == EGL_FALSE) {
        qCWarning(KWIN_OPENGL) << "eglInitialize failed";
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        }
        return nullptr;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        return nullptr;
    }
    qCDebug(KWIN_OPENGL) << "Egl Initialize succeeded";
    if (eglBindAPI(shouldUseOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCCritical(KWIN_OPENGL) << "bind OpenGL API failed";
        return nullptr;
    }
    qCDebug(KWIN_OPENGL) << "EGL version: " << major << "." << minor;

    const auto extensions = QByteArray(eglQueryString(display, EGL_EXTENSIONS)).split(' ');

    const QByteArray requiredExtensions[] = {
        QByteArrayLiteral("EGL_KHR_no_config_context"),
        QByteArrayLiteral("EGL_KHR_surfaceless_context"),
    };
    for (const QByteArray &extensionName : requiredExtensions) {
        if (!extensions.contains(extensionName)) {
            qCWarning(KWIN_OPENGL) << extensionName << "extension is unsupported";
            return nullptr;
        }
    }

    return std::make_unique<EglDisplay>(display, extensions, owning);
}

static std::optional<dev_t> devIdForFileName(const QString &path)
{
    auto device = DrmDevice::open(path);
    if (device) {
        return device->deviceId();
    } else {
        qCWarning(KWIN_OPENGL, "couldn't find dev node for drm device %s", qPrintable(path));
        return std::nullopt;
    }
}

EglDisplay::EglDisplay(::EGLDisplay display, const QList<QByteArray> &extensions, bool owning)
    : m_handle(display)
    , m_extensions(extensions)
    , m_owning(owning)
    , m_renderNode(determineRenderNode())
    , m_renderDevNode(devIdForFileName(m_renderNode))
    , m_supportsBufferAge(extensions.contains(QByteArrayLiteral("EGL_EXT_buffer_age")) && qgetenv("KWIN_USE_BUFFER_AGE") != "0")
    , m_supportsNativeFence(extensions.contains(QByteArrayLiteral("EGL_ANDROID_native_fence_sync"))
                            && extensions.contains(QByteArrayLiteral("EGL_KHR_wait_sync")))
{
    m_functions.createImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    m_functions.destroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    m_functions.queryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
    m_functions.queryDmaBufModifiersEXT = reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));

    m_importFormats = queryImportFormats();
}

EglDisplay::~EglDisplay()
{
    if (m_owning) {
        eglTerminate(m_handle);
    }
}

QList<QByteArray> EglDisplay::extensions() const
{
    return m_extensions;
}

::EGLDisplay EglDisplay::handle() const
{
    return m_handle;
}

bool EglDisplay::hasExtension(const QByteArray &name) const
{
    return m_extensions.contains(name);
}

static bool checkExtension(const QByteArrayView extensions, const QByteArrayView extension)
{
    for (int i = 0; i < extensions.size();) {
        if (extensions[i] == ' ') {
            i++;
            continue;
        }
        int next = extensions.indexOf(' ', i);
        if (next == -1) {
            next = extensions.size();
        }

        const int size = next - i;
        if (extension.size() == size && extensions.sliced(i, size) == extension) {
            return true;
        }

        i = next;
    }

    return false;
}

QString EglDisplay::renderNode() const
{
    return m_renderNode;
}

bool EglDisplay::supportsBufferAge() const
{
    return m_supportsBufferAge;
}

bool EglDisplay::supportsNativeFence() const
{
    return m_supportsNativeFence;
}

EGLImageKHR EglDisplay::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    QList<EGLint> attribs;
    attribs.reserve(6 + dmabuf.planeCount * 10 + 1);

    attribs << EGL_WIDTH << dmabuf.width
            << EGL_HEIGHT << dmabuf.height
            << EGL_LINUX_DRM_FOURCC_EXT << dmabuf.format;

    attribs << EGL_DMA_BUF_PLANE0_FD_EXT << dmabuf.fd[0].get()
            << EGL_DMA_BUF_PLANE0_OFFSET_EXT << dmabuf.offset[0]
            << EGL_DMA_BUF_PLANE0_PITCH_EXT << dmabuf.pitch[0];
    if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
        attribs << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
    }

    if (dmabuf.planeCount > 1) {
        attribs << EGL_DMA_BUF_PLANE1_FD_EXT << dmabuf.fd[1].get()
                << EGL_DMA_BUF_PLANE1_OFFSET_EXT << dmabuf.offset[1]
                << EGL_DMA_BUF_PLANE1_PITCH_EXT << dmabuf.pitch[1];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 2) {
        attribs << EGL_DMA_BUF_PLANE2_FD_EXT << dmabuf.fd[2].get()
                << EGL_DMA_BUF_PLANE2_OFFSET_EXT << dmabuf.offset[2]
                << EGL_DMA_BUF_PLANE2_PITCH_EXT << dmabuf.pitch[2];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 3) {
        attribs << EGL_DMA_BUF_PLANE3_FD_EXT << dmabuf.fd[3].get()
                << EGL_DMA_BUF_PLANE3_OFFSET_EXT << dmabuf.offset[3]
                << EGL_DMA_BUF_PLANE3_PITCH_EXT << dmabuf.pitch[3];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    attribs << EGL_NONE;

    return createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
}

EGLImageKHR EglDisplay::importDmaBufAsImage(const DmaBufAttributes &dmabuf, int plane, int format, const QSize &size) const
{
    QList<EGLint> attribs;
    attribs.reserve(6 + 1 * 10 + 1);

    attribs << EGL_WIDTH << size.width()
            << EGL_HEIGHT << size.height()
            << EGL_LINUX_DRM_FOURCC_EXT << format;

    attribs << EGL_DMA_BUF_PLANE0_FD_EXT << dmabuf.fd[plane].get()
            << EGL_DMA_BUF_PLANE0_OFFSET_EXT << dmabuf.offset[plane]
            << EGL_DMA_BUF_PLANE0_PITCH_EXT << dmabuf.pitch[plane];
    if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
        attribs << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
    }
    attribs << EGL_NONE;

    return createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
}

QHash<uint32_t, EglDisplay::DrmFormatInfo> EglDisplay::allSupportedDrmFormats() const
{
    return m_importFormats;
}

QHash<uint32_t, QList<uint64_t>> EglDisplay::nonExternalOnlySupportedDrmFormats() const
{
    QHash<uint32_t, QList<uint64_t>> ret;
    ret.reserve(m_importFormats.size());
    for (auto it = m_importFormats.constBegin(), itEnd = m_importFormats.constEnd(); it != itEnd; ++it) {
        ret[it.key()] = it->nonExternalOnlyModifiers;
    }
    return ret;
}

bool EglDisplay::isExternalOnly(uint32_t format, uint64_t modifier) const
{
    if (const auto it = m_importFormats.find(format); it != m_importFormats.end()) {
        return it->externalOnlyModifiers.contains(modifier);
    } else {
        return false;
    }
}

QHash<uint32_t, EglDisplay::DrmFormatInfo> EglDisplay::queryImportFormats() const
{
    if (!hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import")) || !hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        return {};
    }

    if (m_functions.queryDmaBufFormatsEXT == nullptr) {
        return {};
    }

    EGLint count = 0;
    EGLBoolean success = m_functions.queryDmaBufFormatsEXT(m_handle, 0, nullptr, &count);
    if (!success || count == 0) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT failed!" << getEglErrorString();
        return {};
    }
    QList<uint32_t> formats(count);
    if (!m_functions.queryDmaBufFormatsEXT(m_handle, count, (EGLint *)formats.data(), &count)) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT with count" << count << "failed!" << getEglErrorString();
        return {};
    }
    QHash<uint32_t, DrmFormatInfo> ret;
    for (const auto format : std::as_const(formats)) {
        if (m_functions.queryDmaBufModifiersEXT != nullptr) {
            EGLint count = 0;
            const EGLBoolean success = m_functions.queryDmaBufModifiersEXT(m_handle, format, 0, nullptr, nullptr, &count);
            if (success && count > 0) {
                DrmFormatInfo drmFormatInfo;
                drmFormatInfo.allModifiers.resize(count);
                QList<EGLBoolean> externalOnly(count);
                if (m_functions.queryDmaBufModifiersEXT(m_handle, format, count, drmFormatInfo.allModifiers.data(), externalOnly.data(), &count)) {
                    drmFormatInfo.externalOnlyModifiers = drmFormatInfo.allModifiers;
                    drmFormatInfo.nonExternalOnlyModifiers = drmFormatInfo.allModifiers;
                    for (int i = drmFormatInfo.allModifiers.size() - 1; i >= 0; i--) {
                        if (externalOnly[i]) {
                            drmFormatInfo.nonExternalOnlyModifiers.removeAll(drmFormatInfo.allModifiers[i]);
                        } else {
                            drmFormatInfo.externalOnlyModifiers.removeAll(drmFormatInfo.allModifiers[i]);
                        }
                    }
                    if (!drmFormatInfo.allModifiers.empty()) {
                        if (!drmFormatInfo.allModifiers.contains(DRM_FORMAT_MOD_INVALID)) {
                            drmFormatInfo.allModifiers.push_back(DRM_FORMAT_MOD_INVALID);
                            if (!drmFormatInfo.nonExternalOnlyModifiers.empty()) {
                                drmFormatInfo.nonExternalOnlyModifiers.push_back(DRM_FORMAT_MOD_INVALID);
                            } else {
                                drmFormatInfo.externalOnlyModifiers.push_back(DRM_FORMAT_MOD_INVALID);
                            }
                        }
                        ret.insert(format, drmFormatInfo);
                    }
                    continue;
                }
            }
        }
        DrmFormatInfo drmFormat;
        drmFormat.allModifiers = {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR};
        drmFormat.nonExternalOnlyModifiers = {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR};
        ret.insert(format, drmFormat);
    }
    return ret;
}

QString EglDisplay::determineRenderNode() const
{
    const char *clientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (checkExtension(clientExtensions, "EGL_EXT_device_query")) {
        EGLAttrib eglDeviceAttrib;
        if (eglQueryDisplayAttribEXT(m_handle, EGL_DEVICE_EXT, &eglDeviceAttrib)) {
            EGLDeviceEXT eglDevice = reinterpret_cast<EGLDeviceEXT>(eglDeviceAttrib);

            const char *deviceExtensions = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
            if (checkExtension(deviceExtensions, "EGL_EXT_device_drm_render_node")) {
                if (const char *node = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT)) {
                    return QString::fromLocal8Bit(node);
                }
            }
            if (checkExtension(deviceExtensions, "EGL_EXT_device_drm")) {
                // Fallback to display device.
                if (const char *node = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT)) {
                    return QString::fromLocal8Bit(node);
                }
            }
        }
    }
    return QString();
}

std::optional<dev_t> EglDisplay::renderDevNode() const
{
    return m_renderDevNode;
}

EGLImageKHR EglDisplay::createImage(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) const
{
    Q_ASSERT(m_functions.createImageKHR);
    return m_functions.createImageKHR(m_handle, ctx, target, buffer, attrib_list);
}

void EglDisplay::destroyImage(EGLImageKHR image) const
{
    Q_ASSERT(m_functions.destroyImageKHR);
    m_functions.destroyImageKHR(m_handle, image);
}
}
