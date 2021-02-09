/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fbvsyncmonitor.h"

#include <QThread>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace KWin
{

FramebufferVsyncMonitorHelper::FramebufferVsyncMonitorHelper(int fileDescriptor, QObject *parent)
    : QObject(parent)
    , m_fileDescriptor(fileDescriptor)
{
}

FramebufferVsyncMonitorHelper::~FramebufferVsyncMonitorHelper()
{
    close(m_fileDescriptor);
}

void FramebufferVsyncMonitorHelper::poll()
{
    if (ioctl(m_fileDescriptor, FBIO_WAITFORVSYNC)) {
        emit errorOccurred();
    } else {
        emit vblankOccurred(std::chrono::steady_clock::now().time_since_epoch());
    }
}

FramebufferVsyncMonitor::FramebufferVsyncMonitor(int fileDescriptor, QObject *parent)
    : VsyncMonitor(parent)
    , m_thread(new QThread)
    , m_helper(new FramebufferVsyncMonitorHelper(fileDescriptor))
{
    m_helper->moveToThread(m_thread);

    connect(m_helper, &FramebufferVsyncMonitorHelper::errorOccurred,
            this, &FramebufferVsyncMonitor::errorOccurred);
    connect(m_helper, &FramebufferVsyncMonitorHelper::vblankOccurred,
            this, &FramebufferVsyncMonitor::vblankOccurred);

    m_thread->setObjectName(QStringLiteral("vsync event monitor"));
    m_thread->start();
}

FramebufferVsyncMonitor::~FramebufferVsyncMonitor()
{
    m_thread->quit();
    m_thread->wait();

    delete m_helper;
    delete m_thread;
}

void FramebufferVsyncMonitor::arm()
{
    QMetaObject::invokeMethod(m_helper, &FramebufferVsyncMonitorHelper::poll);
}

FramebufferVsyncMonitor *FramebufferVsyncMonitor::create(int fileDescriptor, QObject *parent)
{
    const int threadFileDescriptor = dup(fileDescriptor);
    if (threadFileDescriptor == -1) {
        return nullptr;
    }
    return new FramebufferVsyncMonitor(threadFileDescriptor, parent);
}

} // namespace KWin
