/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/vsyncmonitor.h"

#include <epoxy/glx.h>
#include <fixx11h.h>

#include <QThread>

namespace KWin
{

/**
 * The SGIVideoSyncVsyncMonitorHelper class is responsible for waiting for vsync events on the
 * root window. Note that the helper runs on a separate thread.
 */
class SGIVideoSyncVsyncMonitorHelper : public QObject
{
    Q_OBJECT

public:
    explicit SGIVideoSyncVsyncMonitorHelper();
    ~SGIVideoSyncVsyncMonitorHelper() override;

    bool isValid() const;

public Q_SLOTS:
    void poll();

Q_SIGNALS:
    void errorOccurred();
    void vblankOccurred(std::chrono::nanoseconds timestamp);

private:
    ::Display *m_display = nullptr;
    ::Window m_dummyWindow = 0;
    GLXContext m_localContext = 0;
    GLXDrawable m_drawable = 0;
};

/**
 * The SGIVideoSyncVsyncMonitor class monitors vblank events using the GLX_SGI_video_sync
 * extension.
 *
 * Vblank events are monitored in a separated thread to avoid blocking the main thread. In
 * order to avoid locking up the main X11 connection, the worker thread establishes its own
 * X11 connection.
 */
class SGIVideoSyncVsyncMonitor : public VsyncMonitor
{
    Q_OBJECT

public:
    static std::unique_ptr<SGIVideoSyncVsyncMonitor> create();
    ~SGIVideoSyncVsyncMonitor() override;

    bool isValid() const;

public Q_SLOTS:
    void arm() override;

private:
    explicit SGIVideoSyncVsyncMonitor();

    QThread m_thread;
    SGIVideoSyncVsyncMonitorHelper m_helper;
};

} // namespace KWin
