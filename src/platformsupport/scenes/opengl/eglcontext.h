#pragma once

#include "core/dmabufattributes.h"
#include "kwin_export.h"
#include "kwingltexture.h"

#include <QByteArray>
#include <QList>
#include <epoxy/egl.h>

namespace KWin
{

class KWinEglDisplay;

class KWIN_EXPORT KWinEglContext
{
public:
    ~KWinEglContext();

    bool init(KWinEglDisplay *display, EGLConfig config, EGLContext sharedContext);
    bool makeCurrent(EGLSurface surface = EGL_NO_SURFACE) const;
    void doneCurrent() const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf) const;
    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;

    KWinEglDisplay *displayObject() const;
    EGLDisplay display() const;
    EGLContext context() const;
    EGLConfig config() const;
    bool isValid() const;

    void destroy();

private:
    EGLContext createContext(KWinEglDisplay *display, EGLConfig config, EGLContext sharedContext) const;

    KWinEglDisplay *m_display = nullptr;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = EGL_NO_CONFIG_KHR;
};

}
