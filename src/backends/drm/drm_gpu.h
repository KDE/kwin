/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DRM_GPU_H
#define DRM_GPU_H

#include "drm_pipeline.h"

#include <QPointer>
#include <QSize>
#include <QSocketNotifier>
#include <QVector>
#include <qobject.h>

#include <epoxy/egl.h>
#include <sys/types.h>

struct gbm_device;

namespace KWaylandServer
{
class DrmLeaseDeviceV1Interface;
class DrmLeaseV1Interface;
}

namespace KWin
{

class DrmOutput;
class DrmObject;
class DrmCrtc;
class DrmConnector;
class DrmPlane;
class DrmBackend;
class EglGbmBackend;
class DrmAbstractOutput;
class DrmRenderBackend;
class DrmVirtualOutput;

class DrmGpu : public QObject
{
    Q_OBJECT
public:
    DrmGpu(DrmBackend *backend, const QString &devNode, int fd, dev_t deviceId);
    ~DrmGpu();

    int fd() const;
    dev_t deviceId() const;
    QString devNode() const;

    bool isRemoved() const;
    void setRemoved();

    bool atomicModeSetting() const;
    bool addFB2ModifiersSupported() const;
    bool isNVidia() const;
    gbm_device *gbmDevice() const;
    EGLDisplay eglDisplay() const;
    DrmBackend *platform() const;
    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;
    QSize cursorSize() const;

    QVector<DrmVirtualOutput *> virtualOutputs() const;
    QVector<DrmOutput *> drmOutputs() const;
    const QVector<DrmPipeline *> pipelines() const;

    void setEglDisplay(EGLDisplay display);

    bool updateOutputs();
    void removeOutputs();

    DrmVirtualOutput *createVirtualOutput(const QString &name, const QSize &size, double scale);
    void removeVirtualOutput(DrmVirtualOutput *output);

    DrmPipeline::Error testPendingConfiguration();
    bool needsModeset() const;
    bool maybeModeset();

    void releaseBuffers();
    void recreateSurfaces();

Q_SIGNALS:
    void outputAdded(DrmAbstractOutput *output);
    void outputRemoved(DrmAbstractOutput *output);

private:
    void dispatchEvents();
    DrmOutput *findOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void initDrmResources();
    void waitIdle();

    DrmPipeline::Error checkCrtcAssignment(QVector<DrmConnector *> connectors, const QVector<DrmCrtc *> &crtcs);
    DrmPipeline::Error testPipelines();
    QVector<DrmObject *> unusedObjects() const;

    void handleLeaseRequest(KWaylandServer::DrmLeaseV1Interface *leaseRequest);
    void handleLeaseRevoked(KWaylandServer::DrmLeaseV1Interface *lease);

    static void pageFlipHandler(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, unsigned int crtc_id, void *user_data);

    const int m_fd;
    const dev_t m_deviceId;
    const QString m_devNode;
    bool m_atomicModeSetting;
    bool m_addFB2ModifiersSupported = false;
    bool m_isNVidia;
    bool m_isVirtualMachine;
    bool m_isRemoved = false;
    clockid_t m_presentationClock;
    gbm_device *m_gbmDevice;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    DrmBackend *const m_platform;

    std::vector<std::unique_ptr<DrmPlane>> m_planes;
    std::vector<std::unique_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::shared_ptr<DrmConnector>> m_connectors;
    QVector<DrmObject *> m_allObjects;
    QVector<DrmPipeline *> m_pipelines;

    QVector<DrmOutput *> m_drmOutputs;
    QVector<DrmVirtualOutput *> m_virtualOutputs;
    KWaylandServer::DrmLeaseDeviceV1Interface *m_leaseDevice = nullptr;

    QSocketNotifier *m_socketNotifier = nullptr;
    QSize m_cursorSize;
};

}

#endif // DRM_GPU_H
