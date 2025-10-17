/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/drmdevice.h"
#include "drm_buffer.h"
#include "drm_pipeline.h"
#include "utils/filedescriptor.h"
#include "utils/version.h"

#include <QList>
#include <QPointer>
#include <QSize>
#include <QSocketNotifier>
#include <QTimer>

#include <chrono>
#include <epoxy/egl.h>
#include <sys/types.h>

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
class OutputFrame;
class DrmCommit;

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
    /**
     * This should always be longer than any real pageflip can take, even with PSR and modesets
     */
    static constexpr std::chrono::milliseconds s_pageflipTimeout = std::chrono::seconds(1);

    DrmGpu(DrmBackend *backend, int fd, std::unique_ptr<DrmDevice> &&device);
    ~DrmGpu();

    int fd() const;
    DrmDevice *drmDevice() const;

    bool isRemoved() const;
    void setRemoved();
    void setActive(bool active);
    bool isActive() const;

    bool atomicModeSetting() const;
    bool addFB2ModifiersSupported() const;
    bool forceLowBandwidthMode() const;
    bool asyncPageflipSupported() const;
    bool isI915() const;
    bool isNVidia() const;
    bool isAmdgpu() const;
    bool isVmwgfx() const;
    bool isVirtualMachine() const;
    std::optional<Version> nvidiaDriverVersion() const;
    QString driverName() const;
    EglDisplay *eglDisplay() const;
    DrmBackend *platform() const;
    /**
     * Returns the clock from which presentation timestamps are sourced. The returned value
     * can be either CLOCK_MONOTONIC or CLOCK_REALTIME.
     */
    clockid_t presentationClock() const;
    QSize cursorSize() const;

    QList<DrmOutput *> drmOutputs() const;
    const QList<DrmPipeline *> pipelines() const;

    void setEglDisplay(std::unique_ptr<EglDisplay> &&display);

    bool updateOutputs();
    void removeOutputs();

    DrmPipeline::Error testPendingConfiguration();
    void releaseUnusedBuffers();
    bool needsModeset() const;
    void maybeModeset(DrmPipeline *pipeline, const std::shared_ptr<OutputFrame> &frame);

    std::shared_ptr<DrmFramebuffer> importBuffer(GraphicsBuffer *buffer, FileDescriptor &&explicitFence);
    void forgetBuffer(GraphicsBuffer *buf);
    void releaseBuffers();
    void createLayers();
    QList<OutputLayer *> compatibleOutputLayers(Output *output) const;

    FileDescriptor createNonMasterFd() const;
    std::unique_ptr<DrmLease> leaseOutputs(const QList<DrmOutput *> &outputs);
    bool isIdle() const;
    void dispatchEvents();

    void addDefunctCommit(std::unique_ptr<DrmCommit> &&commit);

Q_SIGNALS:
    void activeChanged(bool active);
    void outputAdded(DrmAbstractOutput *output);
    void outputRemoved(DrmAbstractOutput *output);

private:
    DrmOutput *findOutput(quint32 connector);
    void removeOutput(DrmOutput *output);
    void initDrmResources();
    void forgetBufferObject(QObject *buf);
    void doModeset();

    DrmPipeline::Error checkCrtcAssignment(QList<DrmConnector *> connectors, const QList<DrmCrtc *> &crtcs);
    DrmPipeline::Error testPipelines();
    QList<DrmObject *> unusedModesetObjects() const;
    void assignOutputLayers();

    static void pageFlipHandler(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, unsigned int crtc_id, void *user_data);

    const int m_fd;
    const std::unique_ptr<DrmDevice> m_drmDevice;
    bool m_atomicModeSetting;
    bool m_addFB2ModifiersSupported = false;
    bool m_isNVidia;
    bool m_isI915;
    bool m_isAmdgpu;
    bool m_isVmwgfx;
    bool m_isVirtualMachine;
    QString m_driverName;
    bool m_supportsCursorPlaneHotspot = false;
    bool m_asyncPageflipSupported = false;
    bool m_isRemoved = false;
    bool m_isActive = true;
    bool m_forceModeset = false;
    bool m_forceLowBandwidthMode = false;
    clockid_t m_presentationClock;
    std::unique_ptr<EglDisplay> m_eglDisplay;
    DrmBackend *const m_platform;
    std::optional<Version> m_nvidiaDriverVersion;

    std::vector<std::unique_ptr<DrmPlane>> m_planes;
    std::vector<std::unique_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::shared_ptr<DrmConnector>> m_connectors;
    std::unordered_map<DrmConnector *, std::unique_ptr<DrmPipeline>> m_pipelineMap;
    std::unordered_map<DrmPlane *, std::unique_ptr<DrmPipelineLayer>> m_planeLayerMap;
    std::unordered_map<DrmCrtc *, std::unique_ptr<DrmPipelineLayer>> m_legacyLayerMap;
    std::unordered_map<DrmCrtc *, std::unique_ptr<DrmPipelineLayer>> m_legacyCursorLayerMap;
    QList<DrmObject *> m_allObjects;
    QList<DrmPipeline *> m_pipelines;

    QList<DrmOutput *> m_drmOutputs;

    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    QSize m_cursorSize;
    std::unordered_map<DrmPipeline *, std::shared_ptr<OutputFrame>> m_pendingModesetFrames;
    bool m_inModeset = false;
    QHash<GraphicsBuffer *, std::weak_ptr<DrmFramebufferData>> m_fbCache;
    std::vector<std::unique_ptr<DrmCommit>> m_defunctCommits;
    QTimer m_delayedModesetTimer;
};

}

QDebug &operator<<(QDebug &s, const KWin::DrmGpu *gpu);
