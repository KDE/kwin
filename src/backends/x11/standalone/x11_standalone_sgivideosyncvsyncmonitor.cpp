/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_sgivideosyncvsyncmonitor.h"
#include "x11_standalone_glxconvenience.h"
#include "x11_standalone_logging.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif

namespace KWin
{

std::unique_ptr<SGIVideoSyncVsyncMonitor> SGIVideoSyncVsyncMonitor::create()
{
    const char *extensions = glXQueryExtensionsString(QX11Info::display(),
                                                      QX11Info::appScreen());
    if (!strstr(extensions, "GLX_SGI_video_sync")) {
        return nullptr; // GLX_SGI_video_sync is unsupported.
    }

    std::unique_ptr<SGIVideoSyncVsyncMonitor> monitor{new SGIVideoSyncVsyncMonitor()};
    if (monitor->isValid()) {
        return monitor;
    } else {
        return nullptr;
    }
}

SGIVideoSyncVsyncMonitorHelper::SGIVideoSyncVsyncMonitorHelper()
{
    // Establish a new X11 connection to avoid locking up the main X11 connection.
    m_display = XOpenDisplay(DisplayString(QX11Info::display()));
    if (!m_display) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to establish vsync monitor X11 connection";
        return;
    }

    ::Window rootWindow = DefaultRootWindow(m_display);

    const int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        0};

    GLXFBConfig config = chooseGlxFbConfig(m_display, attribs);
    if (!config) {
        qCDebug(KWIN_X11STANDALONE) << "Couldn't find any suitable FBConfig for vsync monitor";
        return;
    }

    XVisualInfo *visualInfo = glXGetVisualFromFBConfig(m_display, config);
    if (!visualInfo) {
        return;
    }

    Visual *visual = visualInfo->visual;
    const int depth = visualInfo->depth;
    XFree(visualInfo);

    Colormap colormap = XCreateColormap(m_display, rootWindow, visual, AllocNone);
    XSetWindowAttributes attributes;
    attributes.colormap = colormap;

    m_dummyWindow = XCreateWindow(m_display, rootWindow, 0, 0, 1, 1, 0, depth,
                                  InputOutput, visual, CWColormap, &attributes);
    XFreeColormap(m_display, colormap);
    if (!m_dummyWindow) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create a dummy window for vsync monitor";
        return;
    }

    m_drawable = glXCreateWindow(m_display, config, m_dummyWindow, nullptr);
    if (!m_drawable) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create GLXWindow for dummy window";
        return;
    }

    m_localContext = glXCreateNewContext(m_display, config, GLX_RGBA_TYPE, 0, true);
    if (!m_localContext) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create opengl context for vsync monitor";
        return;
    }
}

SGIVideoSyncVsyncMonitorHelper::~SGIVideoSyncVsyncMonitorHelper()
{
    if (m_localContext) {
        glXDestroyContext(m_display, m_localContext);
    }
    if (m_drawable) {
        glXDestroyWindow(m_display, m_drawable);
    }
    if (m_dummyWindow) {
        XDestroyWindow(m_display, m_dummyWindow);
    }
    if (m_display) {
        XCloseDisplay(m_display);
    }
}

bool SGIVideoSyncVsyncMonitorHelper::isValid() const
{
    return m_display && m_localContext && m_drawable;
}

void SGIVideoSyncVsyncMonitorHelper::poll()
{
    if (!glXMakeCurrent(m_display, m_drawable, m_localContext)) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to make vsync monitor OpenGL context current";
        Q_EMIT errorOccurred();
        return;
    }

    uint count;

    glXGetVideoSyncSGI(&count);
    glXWaitVideoSyncSGI(2, (count + 1) % 2, &count);

    // Using monotonic clock is inaccurate, but it's still a pretty good estimate.
    Q_EMIT vblankOccurred(std::chrono::steady_clock::now().time_since_epoch());
}

SGIVideoSyncVsyncMonitor::SGIVideoSyncVsyncMonitor()
{
    m_helper.moveToThread(&m_thread);

    connect(&m_helper, &SGIVideoSyncVsyncMonitorHelper::errorOccurred,
            this, &SGIVideoSyncVsyncMonitor::errorOccurred);
    connect(&m_helper, &SGIVideoSyncVsyncMonitorHelper::vblankOccurred,
            this, &SGIVideoSyncVsyncMonitor::vblankOccurred);

    m_thread.setObjectName(QStringLiteral("vsync event monitor"));
    m_thread.start();
}

SGIVideoSyncVsyncMonitor::~SGIVideoSyncVsyncMonitor()
{
    m_thread.quit();
    m_thread.wait();
}

bool SGIVideoSyncVsyncMonitor::isValid() const
{
    return m_helper.isValid();
}

void SGIVideoSyncVsyncMonitor::arm()
{
    QMetaObject::invokeMethod(&m_helper, &SGIVideoSyncVsyncMonitorHelper::poll);
}

} // namespace KWin
