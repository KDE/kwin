/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sgivideosyncvsyncmonitor.h"
#include "logging.h"

#include <QX11Info>

namespace KWin
{

SGIVideoSyncVsyncMonitor *SGIVideoSyncVsyncMonitor::create(QObject *parent)
{
    const char *extensions = glXQueryExtensionsString(QX11Info::display(),
                                                      QX11Info::appScreen());
    if (!strstr(extensions, "GLX_SGI_video_sync")) {
        return nullptr; // GLX_SGI_video_sync is unsupported.
    }

    SGIVideoSyncVsyncMonitor *monitor = new SGIVideoSyncVsyncMonitor(parent);
    if (monitor->isValid()) {
        return monitor;
    }
    delete monitor;
    return nullptr;
}

SGIVideoSyncVsyncMonitorHelper::SGIVideoSyncVsyncMonitorHelper(QObject *parent)
    : QObject(parent)
{
    // Establish a new X11 connection to avoid locking up the main X11 connection.
    m_display = XOpenDisplay(DisplayString(QX11Info::display()));
    if (!m_display) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to establish vsync monitor X11 connection";
        return;
    }

    const int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        0
    };

    int count;
    GLXFBConfig *configs = glXChooseFBConfig(m_display, DefaultScreen(m_display),
                                             attribs, &count);
    if (!count) {
        qCDebug(KWIN_X11STANDALONE) << "Couldn't find any suitable FBConfig for vsync monitor";
        return;
    }

    GLXFBConfig config = configs[0];
    XFree(configs);

    m_localContext = glXCreateNewContext(m_display, config, GLX_RGBA_TYPE, 0, true);
    if (!m_localContext) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create opengl context for vsync monitor";
        return;
    }

    m_drawable = DefaultRootWindow(m_display);
}

SGIVideoSyncVsyncMonitorHelper::~SGIVideoSyncVsyncMonitorHelper()
{
    if (m_localContext) {
        glXDestroyContext(m_display, m_localContext);
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
        emit errorOccurred();
        return;
    }

    uint count;

    glXGetVideoSyncSGI(&count);
    glXWaitVideoSyncSGI(2, (count + 1) % 2, &count);

    // Using monotonic clock is inaccurate, but it's still a pretty good estimate.
    emit vblankOccurred(std::chrono::steady_clock::now().time_since_epoch());
}

SGIVideoSyncVsyncMonitor::SGIVideoSyncVsyncMonitor(QObject *parent)
    : VsyncMonitor(parent)
    , m_thread(new QThread)
    , m_helper(new SGIVideoSyncVsyncMonitorHelper)
{
    m_helper->moveToThread(m_thread);

    connect(m_helper, &SGIVideoSyncVsyncMonitorHelper::errorOccurred,
            this, &SGIVideoSyncVsyncMonitor::errorOccurred);
    connect(m_helper, &SGIVideoSyncVsyncMonitorHelper::vblankOccurred,
            this, &SGIVideoSyncVsyncMonitor::vblankOccurred);

    m_thread->setObjectName(QStringLiteral("vsync event monitor"));
    m_thread->start();
}

SGIVideoSyncVsyncMonitor::~SGIVideoSyncVsyncMonitor()
{
    m_thread->quit();
    m_thread->wait();

    delete m_helper;
    delete m_thread;
}

bool SGIVideoSyncVsyncMonitor::isValid() const
{
    return m_helper->isValid();
}

void SGIVideoSyncVsyncMonitor::arm()
{
    QMetaObject::invokeMethod(m_helper, &SGIVideoSyncVsyncMonitorHelper::poll);
}

} // namespace KWin
