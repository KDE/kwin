
#include "eglcontext.h"
#include "egldisplay.h"
#include "kwineglimagetexture.h"
#include "kwineglutils_p.h"
#include "utils/common.h"
#include "utils/egl_context_attribute_builder.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>

namespace KWin
{

KWinEglContext::~KWinEglContext()
{
    if (m_display && m_context) {
        eglDestroyContext(m_display, m_context);
    }
}

void KWinEglContext::destroy()
{
    if (m_display && m_context) {
        eglDestroyContext(m_display->display(), m_context);
    }
    m_display = nullptr;
    m_context = EGL_NO_CONTEXT;
}

bool KWinEglContext::init(KWinEglDisplay *display, EGLConfig config, EGLContext sharedContext)
{
    m_display = display;
    m_config = config;
    m_context = createContext(display, config, sharedContext);
    return m_context != EGL_NO_CONTEXT;
}

bool KWinEglContext::makeCurrent(EGLSurface surface) const
{
    return eglMakeCurrent(m_display->display(), surface, surface, m_context) == EGL_TRUE;
}

void KWinEglContext::doneCurrent() const
{
    eglMakeCurrent(m_display->display(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

KWinEglDisplay *KWinEglContext::displayObject() const
{
    return m_display;
}

EGLDisplay KWinEglContext::display() const
{
    return m_display->display();
}

EGLContext KWinEglContext::context() const
{
    return m_context;
}

EGLConfig KWinEglContext::config() const
{
    return m_config;
}

bool KWinEglContext::isValid() const
{
    return m_display != nullptr && m_context != EGL_NO_CONTEXT;
}

EGLContext KWinEglContext::createContext(KWinEglDisplay *display, EGLConfig config, EGLContext sharedContext) const
{
    const bool haveRobustness = display->hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = display->hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = display->hasExtension(QByteArrayLiteral("EGL_IMG_context_priority"));
    const bool haveResetOnVideoMemoryPurge = display->hasExtension(QByteArrayLiteral("EGL_NV_robustness_video_memory_purge"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setResetOnVideoMemoryPurge(true);
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }

        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::make_unique<EglOpenGLESContextAttributeBuilder>();
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        if (haveCreateContext) {
            if (haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setResetOnVideoMemoryPurge(true);
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<EglContextAttributeBuilder>();
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<EglContextAttributeBuilder>();
                corePriority->setVersion(3, 1);
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<EglContextAttributeBuilder>();
            core->setVersion(3, 1);
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::make_unique<EglContextAttributeBuilder>();
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::make_unique<EglContextAttributeBuilder>();
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new EglContextAttributeBuilder);
    }

    for (const auto &candidate : candidates) {
        const auto attribs = candidate->build();
        EGLContext ctx = eglCreateContext(display->display(), config, sharedContext, attribs.data());
        if (ctx != EGL_NO_CONTEXT) {
            qCDebug(KWIN_OPENGL) << "Created EGL context with attributes:" << candidate.get();
            return ctx;
        }
    }
    qCCritical(KWIN_OPENGL) << "Create Context failed" << getEglErrorString();
    return EGL_NO_CONTEXT;
}

EGLImageKHR KWinEglContext::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    QVector<EGLint> attribs;
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

    return eglCreateImageKHR(m_display->display(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
}

std::shared_ptr<GLTexture> KWinEglContext::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    EGLImageKHR image = importDmaBufAsImage(attributes);
    if (image != EGL_NO_IMAGE_KHR) {
        return std::make_shared<EGLImageTexture>(m_display->display(), image, GL_RGBA8, QSize(attributes.width, attributes.height));
    } else {
        qCWarning(KWIN_OPENGL) << "Failed to record frame: Error creating EGLImageKHR - " << getEglErrorString();
        return nullptr;
    }
}

}
