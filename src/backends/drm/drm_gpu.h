/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_pipeline.h"
#include "utils/filedescriptor.h"

#include <QList>
#include <QPointer>
#include <QSize>
#include <QSocketNotifier>
#include <qobject.h>

#include <epoxy/egl.h>
#include <sys/types.h>

struct gbm_device;

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
class EglDisplay;
class GraphicsBuffer;
class GraphicsBufferAllocator;

class DrmLease : public QObject
{
    Q_OBJECT
public:
    DrmLease(DrmGpu *gpu, FileDescriptor &&fd, uint32_t lesseeId, const QList<DrmOutput *> &outputs);
    ~DrmLease();

    FileDescriptor &fd();
    uint32_t lesseeId() const;

Q_SIGNALS:
    void revokeRequested();

private:
    DrmGpu *const m_gpu;
    FileDescriptor m_fd;
    const uint32_t m_lesseeId;
    const QList<DrmOutput *> m_outputs;
};

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
    void setActive(bool active);
    bool isActive() const;

    bool atomicModeSetting() const;
    bool addFB2ModifiersSupported() const;
    bool asyncPageflipSupported() const;
    bool isI915() const;
    gbm_device *gbmDevice() const;
    EglDisplay *eglDisplay() const;
    DrmBackend *platform() const;
    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;
    QSize cursorSize() const;

    QList<DrmVirtualOutput *> virtualOutputs() const;
    QList<DrmOutput *> drmOutputs() const;
    const QList<DrmPipeline *> pipelines() const;

    void setEglDisplay(std::unique_ptr<EglDisplay> &&display);

    bool updateOutputs();
    void removeOutputs();

    DrmVirtualOutput *createVirtualOutput(const QString &name, const QSize &size, double scale);
    void removeVirtualOutput(DrmVirtualOutput *output);

    DrmPipeline::Error testPendingConfiguration();
    bool needsModeset() const;
    bool maybeModeset();

    GraphicsBufferAllocator *graphicsBufferAllocator() const;
    std::shared_ptr<DrmFramebuffer> importBuffer(GraphicsBuffer *buffer);
    void releaseBuffers();
    void recreateSurfaces();

    FileDescriptor createNonMasterFd() const;
    std::unique_ptr<DrmLease> leaseOutputs(const QList<DrmOutput *> &outputs);
    void waitIdle();

Q_SIGNALS:
    void activeChanged(bool active);
    void outputAdded(DrmAbstractOutput *output);
    void outputRemoved(DrmAbstractOutput *output);

private:
    void dispatchEvents();
    DrmOutput *findOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void initDrmResources();

    DrmPipeline::Error checkCrtcAssignment(QList<DrmConnector *> connectors, const QList<DrmCrtc *> &crtcs);
    DrmPipeline::Error testPipelines();
    QList<DrmObject *> unusedObjects() const;

    static void pageFlipHandler(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, unsigned int crtc_id, void *user_data);

    const int m_fd;
    const dev_t m_deviceId;
    const QString m_devNode;
    bool m_atomicModeSetting;
    bool m_addFB2ModifiersSupported = false;
    bool m_isNVidia;
    bool m_isI915;
    bool m_isVirtualMachine;
    bool m_supportsCursorPlaneHotspot = false;
    bool m_asyncPageflipSupported = false;
    bool m_isRemoved = false;
    bool m_isActive = true;
    clockid_t m_presentationClock;
    gbm_device *m_gbmDevice;
    FileDescriptor m_gbmFd;
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::unique_ptr<EglDisplay> m_eglDisplay;
    DrmBackend *const m_platform;

    std::vector<std::unique_ptr<DrmPlane>> m_planes;
    std::vector<std::unique_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::shared_ptr<DrmConnector>> m_connectors;
    QList<DrmObject *> m_allObjects;
    QList<DrmPipeline *> m_pipelines;

    QList<DrmOutput *> m_drmOutputs;
    QList<DrmVirtualOutput *> m_virtualOutputs;

    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    QSize m_cursorSize;
};

}
