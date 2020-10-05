/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_gpu.h"

#include "drm_backend.h"
#include "drm_output.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "abstract_egl_backend.h"
#include "logging.h"

#if HAVE_GBM
#include "egl_gbm_backend.h"
#include <gbm.h>
#include "gbm_dmabuf.h"
#endif
// system
#include <algorithm>
#include <unistd.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

namespace KWin
{

DrmGpu::DrmGpu(DrmBackend *backend, QByteArray devNode, int fd, int drmId) : m_backend(backend), m_devNode(devNode), m_fd(fd), m_drmId(drmId), m_atomicModeSetting(false), m_useEglStreams(false), m_gbmDevice(nullptr)
{
    
}

DrmGpu::~DrmGpu()
{
#if HAVE_GBM
    gbm_device_destroy(m_gbmDevice);
#endif
    qDeleteAll(m_crtcs);
    qDeleteAll(m_connectors);
    qDeleteAll(m_planes);
    close(m_fd);
}

void DrmGpu::tryAMS()
{
    m_atomicModeSetting = false;
    if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        bool ams = true;
        QVector<DrmPlane*> planes, overlayPlanes;
        
        DrmScopedPointer<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
        if (!planeResources) {
            qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode on GPU " << m_devNode;
            ams = false;
        }
        if (ams) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting on gpu" << m_devNode;
            qCDebug(KWIN_DRM) << "Number of planes on GPU" << m_devNode << ":" << planeResources->count_planes;
            
            // create the plane objects
            for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
                DrmScopedPointer<drmModePlane> kplane(drmModeGetPlane(m_fd, planeResources->planes[i]));
                DrmPlane *p = new DrmPlane(kplane->plane_id, m_fd);
                if (p->atomicInit()) {
                    planes << p;
                    if (p->type() == DrmPlane::TypeIndex::Overlay) {
                        overlayPlanes << p;
                    }
                } else {
                    delete p;
                }
            }

            if (planes.isEmpty()) {
                qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode on GPU " << m_devNode;
                ams = false;
            }
        }
        if (!ams) {
            for (auto p : planes) {
                delete p;
            }
            planes.clear();
            overlayPlanes.clear();
        }
        m_atomicModeSetting = ams;
        m_planes = planes;
        m_overlayPlanes = overlayPlanes;
    } else {
        qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode on GPU" << m_devNode;
    }
}

bool DrmGpu::updateOutputs()
{
    auto oldConnectors = m_connectors;
    auto oldCrtcs = m_crtcs;
    DrmScopedPointer<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return false;
    }

    for (int i = 0; i < resources->count_connectors; ++i) {
        const uint32_t currentConnector = resources->connectors[i];
        auto it = std::find_if(m_connectors.constBegin(), m_connectors.constEnd(), [currentConnector] (DrmConnector *c) { return c->id() == currentConnector; });
        if (it == m_connectors.constEnd()) {
            auto c = new DrmConnector(currentConnector, m_fd);
            if (m_atomicModeSetting && !c->atomicInit()) {
                delete c;
                continue;
            }
            m_connectors << c;
        } else {
            oldConnectors.removeOne(*it);
        }
    }

    for (int i = 0; i < resources->count_crtcs; ++i) {
        const uint32_t currentCrtc = resources->crtcs[i];
        auto it = std::find_if(m_crtcs.constBegin(), m_crtcs.constEnd(), [currentCrtc] (DrmCrtc *c) { return c->id() == currentCrtc; });
        if (it == m_crtcs.constEnd()) {
            auto c = new DrmCrtc(currentCrtc, m_backend, this, i);
            if (m_atomicModeSetting && !c->atomicInit()) {
                delete c;
                continue;
            }
            m_crtcs << c;
        } else {
            oldCrtcs.removeOne(*it);
        }
    }

    for (auto c : qAsConst(oldConnectors)) {
        m_connectors.removeOne(c);
    }
    for (auto c : qAsConst(oldCrtcs)) {
        m_crtcs.removeOne(c);
    }
    
    QVector<DrmOutput*> connectedOutputs;
    QVector<DrmConnector*> pendingConnectors;

    // split up connected connectors in already or not yet assigned ones
    for (DrmConnector *con : qAsConst(m_connectors)) {
        if (!con->isConnected()) {
            continue;
        }

        if (DrmOutput *o = findOutput(con->id())) {
            connectedOutputs << o;
        } else {
            pendingConnectors << con;
        }
    }

    // check for outputs which got removed
    QVector<DrmOutput*> removedOutputs;
    auto it = m_outputs.begin();
    while (it != m_outputs.end()) {
        if (connectedOutputs.contains(*it)) {
            it++;
            continue;
        }
        DrmOutput *removed = *it;
        it = m_outputs.erase(it);
        removedOutputs.append(removed);
    }
    
    for (DrmConnector *con : qAsConst(pendingConnectors)) {
        DrmScopedPointer<drmModeConnector> connector(drmModeGetConnector(m_fd, con->id()));
        if (!connector) {
            continue;
        }
        if (connector->count_modes == 0) {
            continue;
        }
        bool outputDone = false;

        QVector<uint32_t> encoders = con->encoders();
        for (auto encId : qAsConst(encoders)) {
            DrmScopedPointer<drmModeEncoder> encoder(drmModeGetEncoder(m_fd, encId));
            if (!encoder) {
                continue;
            }
            for (DrmCrtc *crtc : qAsConst(m_crtcs)) {
                if (!(encoder->possible_crtcs & (1 << crtc->resIndex()))) {
                    continue;
                }
                
                // check if crtc isn't used yet -- currently we don't allow multiple outputs on one crtc (cloned mode)
                auto it = std::find_if(connectedOutputs.constBegin(), connectedOutputs.constEnd(),
                    [crtc] (DrmOutput *o) {
                        return o->m_crtc == crtc;
                    }
                );
                if (it != connectedOutputs.constEnd()) {
                    continue;
                }

                // we found a suitable encoder+crtc
                // TODO: we could avoid these lib drm calls if we store all struct data in DrmCrtc and DrmConnector in the beginning
                DrmScopedPointer<drmModeCrtc> modeCrtc(drmModeGetCrtc(m_fd, crtc->id()));
                if (!modeCrtc) {
                    continue;
                }

                DrmOutput *output = new DrmOutput(this->m_backend, this);
                con->setOutput(output);
                output->m_conn = con;
                crtc->setOutput(output);
                output->m_crtc = crtc;

                if (modeCrtc->mode_valid) {
                    output->m_mode = modeCrtc->mode;
                } else {
                    output->m_mode = connector->modes[0];
                }
                qCDebug(KWIN_DRM) << "For new output use mode " << output->m_mode.name;
                if (!output->init(connector.data())) {
                    qCWarning(KWIN_DRM) << "Failed to create output for connector " << con->id();
                    delete output;
                    continue;
                }
                if (!output->initCursor(m_backend->m_cursorSize)) {
                    m_backend->setSoftWareCursor(true);
                }
                qCDebug(KWIN_DRM) << "Found new output with uuid" << output->uuid();

                connectedOutputs << output;
                emit outputAdded(output);
                outputDone = true;
                break;
            }
            if (outputDone) {
                break;
            }
        }
    }
    std::sort(connectedOutputs.begin(), connectedOutputs.end(), [] (DrmOutput *a, DrmOutput *b) { return a->m_conn->id() < b->m_conn->id(); });
    m_outputs = connectedOutputs;
    
    for(DrmOutput *removedOutput : removedOutputs) {
        emit outputRemoved(removedOutput);
        removedOutput->teardown();
        removedOutput->m_crtc = nullptr;
        removedOutput->m_conn = nullptr;
    }
    
    qDeleteAll(oldConnectors);
    qDeleteAll(oldCrtcs);
    return true;
}

DrmOutput *DrmGpu::findOutput(quint32 connector)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [connector] (DrmOutput *o) {
        return o->m_conn->id() == connector;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

}
