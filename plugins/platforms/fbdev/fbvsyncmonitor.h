/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "vsyncmonitor.h"

namespace KWin
{

/**
 * The FramebufferVsyncMonitorHelper class is responsible for waiting vsync events using the
 * FBIO_WAITFORVSYNC ioctl. Note that the helper runs on a separate thread.
 */
class FramebufferVsyncMonitorHelper : public QObject
{
    Q_OBJECT

public:
    explicit FramebufferVsyncMonitorHelper(int fileDescriptor, QObject *parent = nullptr);
    ~FramebufferVsyncMonitorHelper() override;

public Q_SLOTS:
    void poll();

Q_SIGNALS:
    void errorOccurred();
    void vblankOccurred(std::chrono::nanoseconds timestamp);

private:
    int m_fileDescriptor;
};

/**
 * The FramebufferVsyncMonitor class monitors vblank events using the FBIO_WAITFORVSYNC ioctl.
 */
class FramebufferVsyncMonitor : public VsyncMonitor
{
    Q_OBJECT

public:
    /**
     * Creates a fbdev vsync monitor for the device with the specified file descriptor @p fd.
     * This function returns @c null if an error has occurred.
     */
    static FramebufferVsyncMonitor *create(int fd, QObject *parent);

    ~FramebufferVsyncMonitor() override;

public Q_SLOTS:
    void arm() override;

private:
    explicit FramebufferVsyncMonitor(int fileDescriptor, QObject *parent = nullptr);

    QThread *m_thread;
    FramebufferVsyncMonitorHelper *m_helper;
};

} // namespace KWin
