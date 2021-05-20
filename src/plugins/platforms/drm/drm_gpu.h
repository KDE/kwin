/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DRM_GPU_H
#define DRM_GPU_H

#include <qobject.h>
#include <QVector>
#include <QSocketNotifier>
#include <QPointer>

#include <epoxy/egl.h>

#include "drm_buffer.h"
#include "drm_object_plane.h"

struct gbm_device;

namespace KWin
{

class DrmOutput;
class DrmCrtc;
class DrmConnector;
class DrmBackend;
class AbstractEglBackend;

class DrmGpu : public QObject
{
    Q_OBJECT
public:
    DrmGpu(DrmBackend *backend, const QString &devNode, int fd, dev_t deviceId);
    ~DrmGpu();

    // getters
    QVector<DrmOutput*> outputs() const {
        return m_outputs;
    }

    int fd() const {
        return m_fd;
    }

    dev_t deviceId() const {
        return m_deviceId;
    }

    bool atomicModeSetting() const {
        return m_atomicModeSetting;
    }

    bool useEglStreams() const {
        return m_useEglStreams;
    }

    QString devNode() const {
        return m_devNode;
    }

    gbm_device *gbmDevice() const {
        return m_gbmDevice;
    }

    EGLDisplay eglDisplay() const {
        return m_eglDisplay;
    }

    AbstractEglBackend *eglBackend() const;
    void setEglBackend(AbstractEglBackend *eglBackend);

    void setGbmDevice(gbm_device *d) {
        m_gbmDevice = d;
    }

    void setEglDisplay(EGLDisplay display) {
        m_eglDisplay = display;
    }

    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;

    bool addFB2ModifiersSupported() const {
        return m_addFB2ModifiersSupported;
    }

    void waitIdle();

Q_SIGNALS:
    void outputAdded(DrmOutput *output);
    void outputRemoved(DrmOutput *output);
    void outputEnabled(DrmOutput *output);
    void outputDisabled(DrmOutput *output);

protected:
    friend class DrmBackend;
    bool updateOutputs();

private:
    void dispatchEvents();
    DrmPlane *getCompatiblePlane(DrmPlane::TypeIndex typeIndex, DrmCrtc *crtc);
    DrmOutput *findOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void tryAMS();

    DrmBackend* const m_backend;
    QPointer<AbstractEglBackend> m_eglBackend;

    const QString m_devNode;
    QSize m_cursorSize;
    const int m_fd;
    const dev_t m_deviceId;
    bool m_atomicModeSetting;
    bool m_useEglStreams;
    gbm_device* m_gbmDevice;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    clockid_t m_presentationClock;
    QSocketNotifier *m_socketNotifier = nullptr;
    bool m_addFB2ModifiersSupported = false;

    // all planes: primarys, cursors and overlays
    QVector<DrmPlane*> m_planes;
    QVector<DrmPlane*> m_unusedPlanes;
    // crtcs
    QVector<DrmCrtc*> m_crtcs;
    // connectors
    QVector<DrmConnector*> m_connectors;
    // active output pipelines (planes + crtc + encoder + connector)
    QVector<DrmOutput*> m_outputs;
};

}

#endif // DRM_GPU_H
