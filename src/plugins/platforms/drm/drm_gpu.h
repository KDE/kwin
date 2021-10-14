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

    int fd() const;
    dev_t deviceId() const;
    QString devNode() const;

    bool atomicModeSetting() const;
    bool addFB2ModifiersSupported() const;
    bool useEglStreams() const;
    bool isNVidia() const;
    bool isFormatSupported(uint32_t drmFormat) const;
    gbm_device *gbmDevice() const;
    EGLDisplay eglDisplay() const;
    AbstractEglDrmBackend *eglBackend() const;
    DrmBackend *platform() const;
    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;

    QVector<DrmAbstractOutput*> outputs() const;
    const QVector<DrmPipeline*> pipelines() const;

    void setGbmDevice(gbm_device *d);
    void setEglDisplay(EGLDisplay display);
    void setEglBackend(AbstractEglDrmBackend *eglBackend);

    void waitIdle();
    bool updateOutputs();
    DrmVirtualOutput *createVirtualOutput();
    void removeVirtualOutput(DrmVirtualOutput *output);

Q_SIGNALS:
    void outputAdded(DrmAbstractOutput *output);
    void outputRemoved(DrmAbstractOutput *output);
    void outputEnabled(DrmAbstractOutput *output);
    void outputDisabled(DrmAbstractOutput *output);

private:
    void dispatchEvents();
    DrmOutput *findOutput(quint32 connector);
    DrmLeaseOutput *findLeaseOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void removeLeaseOutput(DrmLeaseOutput *output);
    void initDrmResources();

    QVector<DrmPipeline *> findWorkingCombination(const QVector<DrmPipeline *> &pipelines, QVector<DrmConnector *> connectors, QVector<DrmCrtc *> crtcs);
    bool commitCombination(const QVector<DrmPipeline *> &pipelines);

    void handleLeaseRequest(KWaylandServer::DrmLeaseV1Interface *leaseRequest);
    void handleLeaseRevoked(KWaylandServer::DrmLeaseV1Interface *lease);

    const int m_fd;
    const dev_t m_deviceId;
    const QString m_devNode;
    bool m_atomicModeSetting;
    bool m_useEglStreams;
    bool m_addFB2ModifiersSupported = false;
    bool m_isNVidia;
    clockid_t m_presentationClock;
    gbm_device* m_gbmDevice;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    QPointer<AbstractEglDrmBackend> m_eglBackend;
    DrmBackend* const m_platform;

    QVector<DrmPlane*> m_planes;
    QVector<DrmCrtc*> m_crtcs;
    QVector<DrmConnector*> m_connectors;
    QVector<DrmPipeline*> m_pipelines;

    QVector<DrmOutput*> m_drmOutputs;
    QVector<DrmAbstractOutput*> m_outputs;
    QVector<DrmLeaseOutput*> m_leaseOutputs;
    KWaylandServer::DrmLeaseDeviceV1Interface *m_leaseDevice = nullptr;

    QSocketNotifier *m_socketNotifier = nullptr;
    QSize m_cursorSize;
};

}

#endif // DRM_GPU_H
