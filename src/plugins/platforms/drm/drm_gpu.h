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

namespace KWaylandServer
{
class DrmLeaseDeviceV1Interface;
class DrmLeaseV1Interface;
}

namespace KWin
{

class DrmOutput;
class DrmCrtc;
class DrmConnector;
class DrmBackend;
class AbstractEglDrmBackend;
class DrmPipeline;
class DrmAbstractOutput;
class DrmVirtualOutput;
class DrmLeaseOutput;

class DrmGpu : public QObject
{
    Q_OBJECT
public:
    DrmGpu(DrmBackend *backend, const QString &devNode, int fd, dev_t deviceId);
    ~DrmGpu();

    // getters
    QVector<DrmAbstractOutput*> outputs() const {
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

    AbstractEglDrmBackend *eglBackend() const;
    void setEglBackend(AbstractEglDrmBackend *eglBackend);

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
    DrmBackend *platform() const;
    const QVector<DrmPipeline*> pipelines() const;
    bool isFormatSupported(uint32_t drmFormat) const;

    DrmVirtualOutput *createVirtualOutput();
    void removeVirtualOutput(DrmVirtualOutput *output);

Q_SIGNALS:
    void outputAdded(DrmAbstractOutput *output);
    void outputRemoved(DrmAbstractOutput *output);
    void outputEnabled(DrmAbstractOutput *output);
    void outputDisabled(DrmAbstractOutput *output);

protected:
    friend class DrmBackend;
    bool updateOutputs();

private:
    void dispatchEvents();
    DrmOutput *findOutput(quint32 connector);
    DrmLeaseOutput *findLeaseOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void removeLeaseOutput(DrmLeaseOutput *output);
    void initDrmResources();

    QVector<DrmPipeline *> findWorkingCombination(const QVector<DrmPipeline *> &pipelines, QVector<DrmConnector *> connectors, QVector<DrmCrtc *> crtcs, const QVector<DrmPlane *> &planes);
    bool commitCombination(const QVector<DrmPipeline *> &pipelines);

    void handleLeaseRequest(KWaylandServer::DrmLeaseV1Interface *leaseRequest);
    void handleLeaseRevoked(KWaylandServer::DrmLeaseV1Interface *lease);

    DrmBackend* const m_backend;
    QPointer<AbstractEglDrmBackend> m_eglBackend;

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

    QVector<DrmPlane*> m_planes;
    QVector<DrmCrtc*> m_crtcs;
    QVector<DrmConnector*> m_connectors;
    QVector<DrmPipeline*> m_pipelines;

    QVector<DrmOutput*> m_drmOutputs;
    // includes virtual outputs
    QVector<DrmAbstractOutput*> m_outputs;
    // for DRM leasing:
    QVector<DrmLeaseOutput*> m_leaseOutputs;
    KWaylandServer::DrmLeaseDeviceV1Interface *m_leaseDevice = nullptr;
};

}

#endif // DRM_GPU_H
