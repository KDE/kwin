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
#include "main.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"

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
#include <drm_fourcc.h>

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

    m_addFB2ModifiersSupported = drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &capability) == 0 && capability == 1;
    qCDebug(KWIN_DRM) << "drmModeAddFB2WithModifiers is" << (m_addFB2ModifiersSupported ? "supported" : "not supported") << "on GPU" << m_devNode;

    // find out if this GPU is using the NVidia proprietary driver
    DrmScopedPointer<drmVersion> version(drmGetVersion(fd));
    m_useEglStreams = strstr(version->name, "nvidia-drm");

    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &DrmGpu::dispatchEvents);

    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    static const bool atomicModesetting = !qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS");
    if (atomicModesetting) {
        initDrmResources();
    }
}

DrmGpu::~DrmGpu()
{
    waitIdle();
    const auto outputs = m_outputs;
    for (const auto &output : outputs) {
        if (auto drmOutput = qobject_cast<DrmOutput *>(output)) {
            removeOutput(drmOutput);
        } else {
            removeVirtualOutput(dynamic_cast<DrmVirtualOutput*>(output));
        }
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

void DrmGpu::initDrmResources()
{
    // try atomic mode setting
    if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        DrmScopedPointer<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
        if (planeResources) {
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
            } else {
                m_atomicModeSetting = true;
            }
        } else {
            qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode on GPU " << m_devNode;
            m_atomicModeSetting = false;
        }
    } else {
        qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode on GPU" << m_devNode;
        m_atomicModeSetting = false;
    }

    DrmScopedPointer<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCCritical(KWIN_DRM) << "drmModeGetResources for getting CRTCs failed on GPU" << m_devNode;
        return;
    }
    for (int i = 0; i < resources->count_crtcs; ++i) {
        auto c = new DrmCrtc(this, resources->crtcs[i], i);
        if (!c->init()) {
            delete c;
            continue;
        }
        m_crtcs << c;
    }
}

bool DrmGpu::updateOutputs()
{
    waitIdle();
    DrmScopedPointer<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return false;
    }

    // check for added and removed connectors
    QVector<DrmConnector *> removedConnectors = m_connectors;
    for (int i = 0; i < resources->count_connectors; ++i) {
        const uint32_t currentConnector = resources->connectors[i];
        auto it = std::find_if(m_connectors.constBegin(), m_connectors.constEnd(), [currentConnector] (DrmConnector *c) { return c->id() == currentConnector; });
        if (it == m_connectors.constEnd()) {
            auto c = new DrmConnector(this, currentConnector);
            if (!c->init() || !c->isConnected() || c->isNonDesktop()) {
                delete c;
                continue;
            }
            m_connectors << c;
        } else {
            (*it)->updateProperties();
            if ((*it)->isConnected()) {
                removedConnectors.removeOne(*it);
            }
        }
    }
    for (const auto &connector : qAsConst(removedConnectors)) {
        if (auto output = findOutput(connector->id())) {
            removeOutput(output);
        }
        m_connectors.removeOne(connector);
        delete connector;
    }

    // find unused and connected connectors
    QVector<DrmConnector *> connectedConnectors;
    for (const auto &conn : qAsConst(m_connectors)) {
        auto output = findOutput(conn->id());
        if (conn->isConnected() && !conn->isNonDesktop()) {
            connectedConnectors << conn;
            if (output) {
                output->updateModes();
            }
        } else if (output) {
            removeOutput(output);
        }
    }

    // update crtc properties
    for (const auto &crtc : qAsConst(m_crtcs)) {
        crtc->updateProperties();
    }
    // update plane properties
    for (const auto &plane : qAsConst(m_planes)) {
        plane->updateProperties();
    }

    // stash away current pipelines of active outputs
    QMap<DrmOutput*, DrmPipeline*> oldPipelines;
    for (const auto &output : qAsConst(m_drmOutputs)) {
        if (!output->isEnabled()) {
            // create render resources for findWorkingCombination
            Q_EMIT outputEnabled(output);
        }
        m_pipelines.removeOne(output->pipeline());
        oldPipelines.insert(output, output->pipeline());
        output->setPipeline(nullptr);
    }

    if (m_atomicModeSetting) {
        // sort outputs by being already connected (to any CRTC) so that already working outputs get preferred
        std::sort(connectedConnectors.begin(), connectedConnectors.end(), [](auto c1, auto c2){
            return c1->getProp(DrmConnector::PropertyIndex::CrtcId)->current() > c2->getProp(DrmConnector::PropertyIndex::CrtcId)->current();
        });
    }
    auto config = findWorkingCombination({}, connectedConnectors, m_crtcs, m_planes);
    if (config.isEmpty() && !connectedConnectors.isEmpty()) {
        qCCritical(KWIN_DRM) << "DrmGpu::findWorkingCombination failed to find any functional combinations! Reverting to the old configuration!";
        for (auto it = oldPipelines.begin(); it != oldPipelines.end(); it++) {
            it.value()->setOutput(it.key());
            config << it.value();
        }
    } else {
        for (const auto &pipeline : qAsConst(oldPipelines)) {
            delete pipeline;
        }
    }
    m_pipelines << config;

    for (const auto &pipeline : qAsConst(config)) {
        auto output = pipeline->output();
        if (m_outputs.contains(output)) {
            // restore output properties
            if (output->isEnabled()) {
                output->updateTransform(output->transform());
                if (output->dpmsMode() != AbstractWaylandOutput::DpmsMode::On) {
                    pipeline->setActive(false);
                }
            } else {
                pipeline->setActive(false);
                Q_EMIT outputDisabled(output);
            }
        } else {
            qCDebug(KWIN_DRM).nospace() << "New output on GPU " << m_devNode << ": " << pipeline->connector()->modelName();
            if (!output->initCursor(m_cursorSize)) {
                m_backend->setSoftwareCursorForced(true);
            }
            m_outputs << output;
            m_drmOutputs << output;
            Q_EMIT outputAdded(output);
        }
    }

    return true;
}

QVector<DrmPipeline *> DrmGpu::findWorkingCombination(const QVector<DrmPipeline *> &pipelines, QVector<DrmConnector *> connectors, QVector<DrmCrtc *> crtcs, const QVector<DrmPlane *> &planes)
{
    if (connectors.isEmpty() || crtcs.isEmpty()) {
        // no further pipelines can be added -> test configuration
        if (pipelines.isEmpty() || commitCombination(pipelines)) {
            return pipelines;
        } else {
            return {};
        }
    }
    auto connector = connectors.takeFirst();
    const auto encoders = connector->encoders();

    if (m_atomicModeSetting) {
        // try the crtc that this connector is already connected to first
        std::sort(crtcs.begin(), crtcs.end(), [connector](auto c1, auto c2){
            Q_UNUSED(c2)
            if (connector->getProp(DrmConnector::PropertyIndex::CrtcId)->current() == c1->id()) {
                return true;
            } else {
                return false;
            }
        });
    }

    auto recurse = [this, connector, connectors, crtcs, planes, pipelines] (DrmCrtc *crtc, DrmPlane *primaryPlane) {
        auto pipeline = new DrmPipeline(this, connector, crtc, primaryPlane);
        auto crtcsLeft = crtcs;
        crtcsLeft.removeOne(crtc);
        auto planesLeft = planes;
        planesLeft.removeOne(primaryPlane);
        auto allPipelines = pipelines;
        allPipelines << pipeline;
        auto ret = findWorkingCombination(allPipelines, connectors, crtcsLeft, planesLeft);
        if (ret.isEmpty()) {
            delete pipeline;
        }
        return ret;
    };
    for (const auto &encoderId : encoders) {
        DrmScopedPointer<drmModeEncoder> encoder(drmModeGetEncoder(m_fd, encoderId));
        for (const auto &crtc : qAsConst(crtcs)) {
            if (m_atomicModeSetting) {
                for (const auto &plane : qAsConst(planes)) {
                    if (plane->type() == DrmPlane::TypeIndex::Primary
                        && plane->isCrtcSupported(crtc->pipeIndex())) {
                        if (auto workingPipelines = recurse(crtc, plane); !workingPipelines.isEmpty()) {
                            return workingPipelines;
                        }
                    }
                }
            } else {
                if (auto workingPipelines = recurse(crtc, nullptr); !workingPipelines.isEmpty()) {
                    return workingPipelines;
                }
            }
        }
    }
    return {};
}

bool DrmGpu::commitCombination(const QVector<DrmPipeline *> &pipelines)
{
    for (const auto &pipeline : pipelines) {
        auto output = findOutput(pipeline->connector()->id());
        if (output) {
            output->setPipeline(pipeline);
            pipeline->setOutput(output);
        } else {
            output = new DrmOutput(this, pipeline);
            Q_EMIT outputEnabled(output);// create render resources for the test
        }
        pipeline->setup();
    }

    if (DrmPipeline::commitPipelines(pipelines, DrmPipeline::CommitMode::Test)) {
        return true;
    } else {
        for (const auto &pipeline : qAsConst(pipelines)) {
            if (!m_outputs.contains(pipeline->output())) {
                Q_EMIT outputDisabled(pipeline->output());
                delete pipeline->output();
            }
        }
        return false;
    }
}

DrmOutput *DrmGpu::findOutput(quint32 connector)
{
    auto it = std::find_if(m_drmOutputs.constBegin(), m_drmOutputs.constEnd(), [connector] (DrmOutput *o) {
        return o->connector()->id() == connector;
    });
    if (it != m_drmOutputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

void DrmGpu::waitIdle()
{
    m_socketNotifier->setEnabled(false);
    while (true) {
        const bool idle = std::all_of(m_drmOutputs.constBegin(), m_drmOutputs.constEnd(), [](DrmOutput *output){
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
    auto backend = dynamic_cast<DrmBackend*>(kwinApp()->platform());
    if (!backend) {
        return;
    }
    auto gpu = backend->findGpuByFd(fd);
    if (!gpu) {
        return;
    }
    auto output = static_cast<DrmOutput *>(data);
    if (!gpu->outputs().contains(output)) {
        // output already got deleted
        return;
    }

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
    m_drmOutputs.removeOne(output);
    m_outputs.removeOne(output);
    if (output->isEnabled()) {
        Q_EMIT outputDisabled(output);
    }
    Q_EMIT outputRemoved(output);
    auto pipeline = output->m_pipeline;
    delete output;
    m_pipelines.removeOne(pipeline);
    delete pipeline;
}

AbstractEglDrmBackend *DrmGpu::eglBackend() const
{
    return m_eglBackend;
}

void DrmGpu::setEglBackend(AbstractEglDrmBackend *eglBackend)
{
    m_eglBackend = eglBackend;
}

DrmBackend *DrmGpu::platform() const {
    return m_backend;
}

const QVector<DrmPipeline*> DrmGpu::pipelines() const
{
    return m_pipelines;
}

DrmVirtualOutput *DrmGpu::createVirtualOutput()
{
    auto output = new DrmVirtualOutput(this);
    output->setPlaceholder(true);
    m_outputs << output;
    Q_EMIT outputEnabled(output);
    Q_EMIT outputAdded(output);
    return output;
}

void DrmGpu::removeVirtualOutput(DrmVirtualOutput *output)
{
    if (m_outputs.removeOne(output)) {
        if (output->isEnabled()) {
            Q_EMIT outputDisabled(output);
        }
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

bool DrmGpu::isFormatSupported(uint32_t drmFormat) const
{
    if (!m_atomicModeSetting) {
        return drmFormat == DRM_FORMAT_XRGB8888 || drmFormat == DRM_FORMAT_ARGB8888;
    } else {
        for (const auto &plane : qAsConst(m_planes)) {
            if (plane->type() == DrmPlane::TypeIndex::Primary && !plane->formats().contains(drmFormat)) {
                return false;
            }
        }
        return true;
    }
}

}
