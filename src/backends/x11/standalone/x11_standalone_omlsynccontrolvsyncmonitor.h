/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/vsyncmonitor.h"

#include <epoxy/glx.h>
#include <fixx11h.h>

#include <QThread>
#include <memory>

namespace KWin
{

/**
 * The OMLSyncControlVsyncMonitorHelper class is responsible for waiting vsync events on the
 * root window. Note that the helper runs on a separate thread.
 */
class OMLSyncControlVsyncMonitorHelper : public QObject
{
    Q_OBJECT

public:
    explicit OMLSyncControlVsyncMonitorHelper();
    ~OMLSyncControlVsyncMonitorHelper() override;

    bool isValid() const;

public Q_SLOTS:
    void poll();

Q_SIGNALS:
    void errorOccurred();
    void vblankOccurred(std::chrono::nanoseconds timestamp);

private:
    Display *m_display = nullptr;
    ::Window m_dummyWindow = 0;
    GLXContext m_localContext = 0;
    GLXDrawable m_drawable = 0;
};

/**
 * The OMLSyncControlVsyncMonitor class monitors vblank events using the GLX_OML_sync_control
 * extension.
 *
 * Vblank events are monitored in a separated thread to avoid blocking the main thread. In
 * order to avoid locking up the main X11 connection, the worker thread establishes its own
 * X11 connection.
 */
class OMLSyncControlVsyncMonitor : public VsyncMonitor
{
    Q_OBJECT

public:
    static std::unique_ptr<OMLSyncControlVsyncMonitor> create();
    ~OMLSyncControlVsyncMonitor() override;

    bool isValid() const;

public Q_SLOTS:
    void arm() override;

private:
    explicit OMLSyncControlVsyncMonitor();

    QThread m_thread;
    OMLSyncControlVsyncMonitorHelper m_helper;
};

} // namespace KWin
