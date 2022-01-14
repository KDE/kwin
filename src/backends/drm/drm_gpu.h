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
#include <QSize>

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
    bool isNVidia() const;
    gbm_device *gbmDevice() const;
    EGLDisplay eglDisplay() const;
    EglGbmBackend *eglBackend() const;
    DrmBackend *platform() const;
    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;
    QSize cursorSize() const;

    QVector<DrmAbstractOutput*> outputs() const;
    const QVector<DrmPipeline*> pipelines() const;

    void setEglDisplay(EGLDisplay display);
    void setEglBackend(EglGbmBackend *eglBackend);

    bool updateOutputs();

    enum VirtualOutputMode { Placeholder, Full };
    DrmVirtualOutput *createVirtualOutput(const QString &name, const QSize &size, double scale, VirtualOutputMode mode);
    void removeVirtualOutput(DrmVirtualOutput *output);

    enum class TestMode {
        TestOnly,
        TestWithCrtcReallocation
    };
    bool testPendingConfiguration(TestMode mode = TestMode::TestWithCrtcReallocation);
    bool needsModeset() const;
    bool maybeModeset();

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
    void waitIdle();

    bool checkCrtcAssignment(QVector<DrmConnector*> connectors, QVector<DrmCrtc*> crtcs);
    bool testPipelines();
    QVector<DrmObject*> unusedObjects() const;

    void handleLeaseRequest(KWaylandServer::DrmLeaseV1Interface *leaseRequest);
    void handleLeaseRevoked(KWaylandServer::DrmLeaseV1Interface *lease);

    static void pageFlipHandler(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, unsigned int crtc_id, void *user_data);

    const int m_fd;
    const dev_t m_deviceId;
    const QString m_devNode;
    bool m_atomicModeSetting;
    bool m_addFB2ModifiersSupported = false;
    bool m_isNVidia;
    clockid_t m_presentationClock;
    gbm_device* m_gbmDevice;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    QPointer<EglGbmBackend> m_eglBackend;
    DrmBackend* const m_platform;

    QVector<DrmPlane*> m_planes;
    QVector<DrmCrtc*> m_crtcs;
    QVector<DrmConnector*> m_connectors;
    QVector<DrmObject*> m_allObjects;
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
