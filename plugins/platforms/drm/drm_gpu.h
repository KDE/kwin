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

#include "drm_buffer.h"

struct gbm_device;

namespace KWin
{
    
class DrmOutput;
class DrmPlane;
class DrmCrtc;
class DrmConnector;
class DrmBackend;
class AbstractEglBackend;
    
class DrmGpu : public QObject
{
    Q_OBJECT
public:
    DrmGpu(DrmBackend *backend, QByteArray devNode, int fd, int drmId);
    ~DrmGpu();
    
    // getters
    QVector<DrmOutput*> outputs() const {
        return m_outputs;
    }
    
    int fd() const {
        return m_fd;
    }
    
    int drmId() const {
        return m_drmId;
    }
    
    bool atomicModeSetting() const {
        return m_atomicModeSetting;
    }
    
    QByteArray devNode() const {
        return m_devNode;
    }
    
    gbm_device *gbmDevice() const {
        return m_gbmDevice;
    }
    
    QVector<DrmPlane*> planes() const {
        return m_planes;
    }
    
    void setGbmDevice(gbm_device *d) {
        m_gbmDevice = d;
    }
    
    DrmDumbBuffer *createBuffer(const QSize &size) const {
        return new DrmDumbBuffer(m_fd, size);
    }
    
Q_SIGNALS:
    void outputAdded(DrmOutput *output);
    void outputRemoved(DrmOutput *output);
    void outputEnabled(DrmOutput *output);
    void outputDisabled(DrmOutput *output);
    
protected:
    
    friend class DrmBackend;
    void tryAMS();
    bool updateOutputs();
    
private:
    DrmOutput *findOutput(quint32 connector);
    
    DrmBackend* const m_backend;
    
    const QByteArray m_devNode;
    const int m_fd;
    const int m_drmId;
    bool m_atomicModeSetting;
    bool m_useEglStreams;
    gbm_device* m_gbmDevice;
    
// all available planes: primarys, cursors and overlays
    QVector<DrmPlane*> m_planes;
    QVector<DrmPlane*> m_overlayPlanes;
    // crtcs
    QVector<DrmCrtc*> m_crtcs;
    // connectors
    QVector<DrmConnector*> m_connectors;
    // active output pipelines (planes + crtc + encoder + connector)
    QVector<DrmOutput*> m_outputs;
};

}

#endif // DRM_GPU_H
