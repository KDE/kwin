/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_gpu.h"
#include <config-kwin.h>
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "abstract_egl_backend.h"
#include "logging.h"
#include "session.h"
#include "renderloop_p.h"

#if HAVE_GBM
#include "egl_gbm_backend.h"
#include <gbm.h>
#include "gbm_dmabuf.h"
#endif
// system
#include <algorithm>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

namespace KWin
{

DrmGpu::DrmGpu(DrmBackend *backend, const QString &devNode, int fd, dev_t deviceId)
    : m_backend(backend)
    , m_devNode(devNode)
    , m_fd(fd)
    , m_deviceId(deviceId)
    , m_atomicModeSetting(false)
    , m_gbmDevice(nullptr)
{
    uint64_t capability = 0;

    if (drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &capability) == 0) {
        m_cursorSize.setWidth(capability);
    } else {
        m_cursorSize.setWidth(64);
    }

    if (drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &capability) == 0) {
        m_cursorSize.setHeight(capability);
    } else {
        m_cursorSize.setHeight(64);
    }

    int ret = drmGetCap(fd, DRM_CAP_TIMESTAMP_MONOTONIC, &capability);
    if (ret == 0 && capability == 1) {
        m_presentationClock = CLOCK_MONOTONIC;
    } else {
        m_presentationClock = CLOCK_REALTIME;
    }

    if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_MODIFIERS")) {
        m_addFB2ModifiersSupported = drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &capability) == 0 && capability == 1;
        qCDebug(KWIN_DRM) << "drmModeAddFB2WithModifiers is" << (m_addFB2ModifiersSupported ? "supported" : "not supported") << "on GPU" << m_devNode;
    }

    // find out if this GPU is using the NVidia proprietary driver
    DrmScopedPointer<drmVersion> version(drmGetVersion(fd));
    m_useEglStreams = strstr(version->name, "nvidia-drm");

    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &DrmGpu::dispatchEvents);

    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    static const bool atomicModesetting = !qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS");
    if (atomicModesetting) {
        tryAMS();
    }
}

DrmGpu::~DrmGpu()
{
    waitIdle();
    const auto outputs = m_outputs;
    for (const auto &output : outputs) {
        removeOutput(output);
    }
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(m_eglDisplay);
    }
    qDeleteAll(m_crtcs);
    qDeleteAll(m_connectors);
    qDeleteAll(m_planes);
    delete m_socketNotifier;
#if HAVE_GBM
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
#endif
    m_backend->session()->closeRestricted(m_fd);
}

clockid_t DrmGpu::presentationClock() const
{
    return m_presentationClock;
}

void DrmGpu::tryAMS()
{
    m_atomicModeSetting = false;
    if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        m_atomicModeSetting = true;
        DrmScopedPointer<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
        if (!planeResources) {
            qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode on GPU " << m_devNode;
            m_atomicModeSetting = false;
            return;
        }
        qCDebug(KWIN_DRM) << "Using Atomic Mode Setting on gpu" << m_devNode;
        qCDebug(KWIN_DRM) << "Number of planes on GPU" << m_devNode << ":" << planeResources->count_planes;
        // create the plane objects
        for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
            DrmScopedPointer<drmModePlane> kplane(drmModeGetPlane(m_fd, planeResources->planes[i]));
            DrmPlane *p = new DrmPlane(this, kplane->plane_id);
            if (p->init()) {
                m_planes << p;
            } else {
                delete p;
            }
        }
        if (m_planes.isEmpty()) {
            qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode on GPU " << m_devNode;
            m_atomicModeSetting = false;
        }
        m_unusedPlanes = m_planes;
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
            auto c = new DrmConnector(this, currentConnector);
            if (!c->init()) {
                delete c;
                continue;
            }
            if (c->isNonDesktop()) {
                delete c;
                continue;
            }
            if (!c->isConnected()) {
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
            auto c = new DrmCrtc(this, currentCrtc, i);
            if (!c->init()) {
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
                if (!(encoder->possible_crtcs & (1 << crtc->pipeIndex()))) {
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

                auto primary = getCompatiblePlane(DrmPlane::TypeIndex::Primary, crtc);
                if (m_atomicModeSetting && !primary) {
                    continue;
                }

                DrmOutput *output = new DrmOutput(this->m_backend, this);
                output->m_conn = con;
                output->m_crtc = crtc;
                output->m_primaryPlane = primary;
                output->m_mode = connector->modes[0];

                if (!output->init(connector.data())) {
                    qCWarning(KWIN_DRM) << "Failed to create output for connector " << con->id();
                    delete output;
                    continue;
                }
                if (!output->initCursor(m_cursorSize)) {
                    m_backend->setSoftwareCursorForced(true);
                }
                qCDebug(KWIN_DRM, "For new output %s on GPU %s use mode %dx%d@%d", qPrintable(output->name()), qPrintable(m_devNode), output->m_mode.hdisplay, output->m_mode.vdisplay, output->refreshRate());

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
        removeOutput(removedOutput);
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

DrmPlane *DrmGpu::getCompatiblePlane(DrmPlane::TypeIndex typeIndex, DrmCrtc *crtc)
{
    for (auto plane : m_unusedPlanes) {
        if (plane->type() != typeIndex) {
            continue;
        }
        if (plane->isCrtcSupported(crtc->pipeIndex())) {
            m_unusedPlanes.removeOne(plane);
            return plane;
        }
    }
    return nullptr;
}

void DrmGpu::waitIdle()
{
    m_socketNotifier->setEnabled(false);
    while (true) {
        const bool idle = std::all_of(m_outputs.constBegin(), m_outputs.constEnd(), [](DrmOutput *output){
            return !output->m_pageFlipPending;
        });
        if (idle) {
            break;
        }
        pollfd pfds[1];
        pfds[0].fd = m_fd;
        pfds[0].events = POLLIN;

        const int ready = poll(pfds, 1, 30000);
        if (ready < 0) {
            if (errno != EINTR) {
                qCWarning(KWIN_DRM) << Q_FUNC_INFO << "poll() failed:" << strerror(errno);
                break;
            }
        } else if (ready == 0) {
            qCWarning(KWIN_DRM) << "No drm events for gpu" << m_devNode << "within last 30 seconds";
            break;
        } else {
            dispatchEvents();
        }
    };
    m_socketNotifier->setEnabled(true);
}

static std::chrono::nanoseconds convertTimestamp(const timespec &timestamp)
{
    return std::chrono::seconds(timestamp.tv_sec) + std::chrono::nanoseconds(timestamp.tv_nsec);
}

static std::chrono::nanoseconds convertTimestamp(clockid_t sourceClock, clockid_t targetClock,
                                                 const timespec &timestamp)
{
    if (sourceClock == targetClock) {
        return convertTimestamp(timestamp);
    }

    timespec sourceCurrentTime = {};
    timespec targetCurrentTime = {};

    clock_gettime(sourceClock, &sourceCurrentTime);
    clock_gettime(targetClock, &targetCurrentTime);

    const auto delta = convertTimestamp(sourceCurrentTime) - convertTimestamp(timestamp);
    return convertTimestamp(targetCurrentTime) - delta;
}

static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    Q_UNUSED(fd)
    Q_UNUSED(frame)

    auto output = static_cast<DrmOutput *>(data);

    // The static_cast<> here are for a 32-bit environment where 
    // sizeof(time_t) == sizeof(unsigned int) == 4 . Putting @p sec
    // into a time_t cuts off the most-significant bit (after the
    // year 2038), similarly long can't hold all the bits of an
    // unsigned multiplication.
    std::chrono::nanoseconds timestamp = convertTimestamp(output->gpu()->presentationClock(),
                                                          CLOCK_MONOTONIC,
                                                          { static_cast<time_t>(sec), static_cast<long>(usec * 1000) });
    if (timestamp == std::chrono::nanoseconds::zero()) {
        qCDebug(KWIN_DRM, "Got invalid timestamp (sec: %u, usec: %u) on output %s",
                sec, usec, qPrintable(output->name()));
        timestamp = std::chrono::steady_clock::now().time_since_epoch();
    }

    output->pageFlipped();
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(output->renderLoop());
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

void DrmGpu::dispatchEvents()
{
    if (!m_backend->session()->isActive()) {
        return;
    }
    drmEventContext context = {};
    context.version = 2;
    context.page_flip_handler = pageFlipHandler;
    drmHandleEvent(m_fd, &context);
}

void DrmGpu::removeOutput(DrmOutput *output)
{
    m_outputs.removeOne(output);
    emit outputRemoved(output);
    output->teardown();
    output->m_crtc = nullptr;
    m_connectors.removeOne(output->m_conn);
    delete output->m_conn;
    output->m_conn = nullptr;
    if (output->m_primaryPlane) {
        m_unusedPlanes << output->m_primaryPlane;
    }
}

AbstractEglBackend *DrmGpu::eglBackend() const
{
    return m_eglBackend;
}

void DrmGpu::setEglBackend(AbstractEglBackend *eglBackend)
{
    m_eglBackend = eglBackend;
}

}
