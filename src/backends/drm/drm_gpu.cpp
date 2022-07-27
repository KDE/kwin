/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_gpu.h"

#include <config-kwin.h>

#include "abstract_egl_backend.h"
#include "drm_backend.h"
#include "drm_egl_backend.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "gbm_dmabuf.h"
#include "renderloop_p.h"
#include "session.h"
#include "wayland/drmleasedevice_v1_interface.h"
#include "wayland_server.h"
// system
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
// drm
#include <drm_fourcc.h>
#include <gbm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

DrmGpu::DrmGpu(DrmBackend *backend, const QString &devNode, int fd, dev_t deviceId)
    : m_fd(fd)
    , m_deviceId(deviceId)
    , m_devNode(devNode)
    , m_atomicModeSetting(false)
    , m_gbmDevice(nullptr)
    , m_platform(backend)
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

    // find out what driver this kms device is using
    DrmUniquePtr<drmVersion> version(drmGetVersion(fd));
    m_isNVidia = strstr(version->name, "nvidia-drm");
    m_isVirtualMachine = strstr(version->name, "virtio") || strstr(version->name, "qxl")
        || strstr(version->name, "vmwgfx") || strstr(version->name, "vboxvideo");
    m_gbmDevice = gbm_create_device(m_fd);

    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &DrmGpu::dispatchEvents);

    initDrmResources();

    m_leaseDevice = new KWaylandServer::DrmLeaseDeviceV1Interface(waylandServer()->display(), [this] {
        char *path = drmGetDeviceNameFromFd2(m_fd);
        FileDescriptor fd{open(path, O_RDWR | O_CLOEXEC)};
        if (!fd.isValid()) {
            qCWarning(KWIN_DRM) << "Could not open DRM fd for leasing!" << strerror(errno);
        } else {
            if (drmIsMaster(fd.get())) {
                if (drmDropMaster(fd.get()) != 0) {
                    qCWarning(KWIN_DRM) << "Could not create a non-master DRM fd for leasing!" << strerror(errno);
                    return FileDescriptor{};
                }
            }
        }
        return fd;
    });
    connect(m_leaseDevice, &KWaylandServer::DrmLeaseDeviceV1Interface::leaseRequested, this, &DrmGpu::handleLeaseRequest);
    connect(m_leaseDevice, &KWaylandServer::DrmLeaseDeviceV1Interface::leaseRevoked, this, &DrmGpu::handleLeaseRevoked);
    connect(m_platform, &DrmBackend::activeChanged, m_leaseDevice, [this]() {
        if (!m_platform->isActive()) {
            // when we gain drm master we want to update outputs first and only then notify the lease device
            m_leaseDevice->setDrmMaster(false);
        }
    });
}

DrmGpu::~DrmGpu()
{
    waitIdle();
    const auto outputs = m_outputs;
    for (const auto &output : outputs) {
        if (auto drmOutput = qobject_cast<DrmOutput *>(output)) {
            removeOutput(drmOutput);
        } else {
            removeVirtualOutput(dynamic_cast<DrmVirtualOutput *>(output));
        }
    }
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(m_eglDisplay);
    }
    m_crtcs.clear();
    m_connectors.clear();
    m_planes.clear();
    delete m_socketNotifier;
    delete m_leaseDevice;
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
    m_platform->session()->closeRestricted(m_fd);
}

clockid_t DrmGpu::presentationClock() const
{
    return m_presentationClock;
}

void DrmGpu::initDrmResources()
{
    // try atomic mode setting
    bool isEnvVarSet = false;
    bool noAMS = qEnvironmentVariableIntValue("KWIN_DRM_NO_AMS", &isEnvVarSet) != 0 && isEnvVarSet;
    if (m_isVirtualMachine && !isEnvVarSet) {
        qCWarning(KWIN_DRM, "Atomic Mode Setting disabled on GPU %s because of cursor offset issues in virtual machines", qPrintable(m_devNode));
    } else if (noAMS) {
        qCWarning(KWIN_DRM) << "Atomic Mode Setting requested off via environment variable. Using legacy mode on GPU" << m_devNode;
    } else if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
        qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode on GPU" << m_devNode;
    } else {
        DrmUniquePtr<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
        if (planeResources) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting on gpu" << m_devNode;
            qCDebug(KWIN_DRM) << "Number of planes on GPU" << m_devNode << ":" << planeResources->count_planes;
            // create the plane objects
            for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
                DrmUniquePtr<drmModePlane> kplane(drmModeGetPlane(m_fd, planeResources->planes[i]));
                auto plane = std::make_unique<DrmPlane>(this, kplane->plane_id);
                if (plane->init()) {
                    m_allObjects << plane.get();
                    m_planes.push_back(std::move(plane));
                }
            }
            if (m_planes.empty()) {
                qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode on GPU " << m_devNode;
            }
        } else {
            qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode on GPU " << m_devNode;
        }
    }
    m_atomicModeSetting = !m_planes.empty();

    DrmUniquePtr<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCCritical(KWIN_DRM) << "drmModeGetResources for getting CRTCs failed on GPU" << m_devNode;
        return;
    }
    QVector<DrmPlane *> assignedPlanes;
    for (int i = 0; i < resources->count_crtcs; ++i) {
        uint32_t crtcId = resources->crtcs[i];
        DrmPlane *primary = nullptr;
        DrmPlane *cursor = nullptr;
        for (const auto &plane : m_planes) {
            if (plane->isCrtcSupported(i) && !assignedPlanes.contains(plane.get())) {
                if (plane->type() == DrmPlane::TypeIndex::Primary) {
                    if (!primary || primary->getProp(DrmPlane::PropertyIndex::CrtcId)->pending() == crtcId) {
                        primary = plane.get();
                    }
                } else if (plane->type() == DrmPlane::TypeIndex::Cursor) {
                    if (!cursor || cursor->getProp(DrmPlane::PropertyIndex::CrtcId)->pending() == crtcId) {
                        cursor = plane.get();
                    }
                }
            }
        }
        if (m_atomicModeSetting && !primary) {
            qCWarning(KWIN_DRM) << "Could not find a suitable primary plane for crtc" << resources->crtcs[i];
            continue;
        }
        assignedPlanes.push_back(primary);
        assignedPlanes.push_back(cursor);
        auto crtc = std::make_unique<DrmCrtc>(this, crtcId, i, primary, cursor);
        if (!crtc->init()) {
            continue;
        }
        m_allObjects << crtc.get();
        m_crtcs.push_back(std::move(crtc));
    }
}

bool DrmGpu::updateOutputs()
{
    waitIdle();
    DrmUniquePtr<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return false;
    }

    // In principle these things are supposed to be detected through the wayland protocol.
    // In practice SteamVR doesn't always behave correctly
    DrmUniquePtr<drmModeLesseeListRes> lessees{drmModeListLessees(m_fd)};
    for (const auto &output : qAsConst(m_drmOutputs)) {
        if (output->lease()) {
            bool leaseActive = false;
            for (uint i = 0; i < lessees->count; i++) {
                if (lessees->lessees[i] == output->lease()->lesseeId()) {
                    leaseActive = true;
                    break;
                }
            }
            if (!leaseActive) {
                output->lease()->revoke();
            }
        }
    }

    // check for added and removed connectors
    QVector<DrmConnector *> existing;
    QVector<DrmOutput *> addedOutputs;
    for (int i = 0; i < resources->count_connectors; ++i) {
        const uint32_t currentConnector = resources->connectors[i];
        const auto it = std::find_if(m_connectors.begin(), m_connectors.end(), [currentConnector](const auto &connector) {
            return connector->id() == currentConnector;
        });
        if (it == m_connectors.end()) {
            auto conn = std::make_unique<DrmConnector>(this, currentConnector);
            if (!conn->init()) {
                continue;
            }
            existing.push_back(conn.get());
            m_allObjects.push_back(conn.get());
            m_connectors.push_back(std::move(conn));
        } else {
            (*it)->updateProperties();
            existing.push_back(it->get());
        }
    }
    for (auto it = m_connectors.begin(); it != m_connectors.end();) {
        DrmConnector *conn = it->get();
        const auto output = findOutput(conn->id());
        const bool stillExists = existing.contains(conn);
        if (!stillExists || !conn->isConnected()) {
            if (output) {
                removeOutput(output);
            }
            conn->disable();
        } else if (!output) {
            qCDebug(KWIN_DRM, "New %soutput on GPU %s: %s", conn->isNonDesktop() ? "non-desktop " : "", qPrintable(m_devNode), qPrintable(conn->modelName()));
            const auto pipeline = conn->pipeline();
            m_pipelines << pipeline;
            auto output = new DrmOutput(pipeline, m_leaseDevice);
            m_drmOutputs << output;
            m_outputs << output;
            addedOutputs << output;
            Q_EMIT outputAdded(output);
            pipeline->setLayers(m_platform->renderBackend()->createPrimaryLayer(pipeline), m_platform->renderBackend()->createCursorLayer(pipeline));
            pipeline->setActive(!conn->isNonDesktop());
            pipeline->applyPendingChanges();
        }
        if (stillExists) {
            it++;
        } else {
            m_allObjects.removeOne(it->get());
            it = m_connectors.erase(it);
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
    DrmPipeline::Error err = testPendingConfiguration();
    if (err == DrmPipeline::Error::None) {
        for (const auto &pipeline : qAsConst(m_pipelines)) {
            pipeline->applyPendingChanges();
            if (pipeline->output() && !pipeline->crtc()) {
                pipeline->setEnable(false);
                pipeline->output()->setEnabled(false);
            }
        }
    } else if (err == DrmPipeline::Error::NoPermission) {
        for (const auto &pipeline : qAsConst(m_pipelines)) {
            pipeline->revertPendingChanges();
        }
        for (const auto &output : qAsConst(addedOutputs)) {
            removeOutput(output);
            const auto it = std::find_if(m_connectors.begin(), m_connectors.end(), [output](const auto &conn) {
                return conn.get() == output->connector();
            });
            Q_ASSERT(it != m_connectors.end());
            m_allObjects.removeOne(it->get());
            m_connectors.erase(it);
        }
        QTimer::singleShot(50, m_platform, &DrmBackend::updateOutputs);
    } else {
        qCWarning(KWIN_DRM, "Failed to find a working setup for new outputs!");
        for (const auto &pipeline : qAsConst(m_pipelines)) {
            pipeline->revertPendingChanges();
        }
        for (const auto &output : qAsConst(addedOutputs)) {
            output->setEnabled(false);
            output->pipeline()->setEnable(false);
            output->pipeline()->applyPendingChanges();
        }
    }
    m_leaseDevice->setDrmMaster(true);
    // after (potential) lease offer changes, a done event needs to be sent
    // to signal clients to handle the changes
    m_leaseDevice->done();
    return true;
}

DrmPipeline::Error DrmGpu::checkCrtcAssignment(QVector<DrmConnector *> connectors, const QVector<DrmCrtc *> &crtcs)
{
    if (connectors.isEmpty() || crtcs.isEmpty()) {
        if (m_pipelines.isEmpty()) {
            // nothing to do
            return DrmPipeline::Error::None;
        }
        // remaining connectors can't be powered
        for (const auto &conn : qAsConst(connectors)) {
            qCWarning(KWIN_DRM) << "disabling connector" << conn->modelName() << "without a crtc";
            conn->pipeline()->setCrtc(nullptr);
        }
        return testPipelines();
    }
    auto connector = connectors.takeFirst();
    auto pipeline = connector->pipeline();
    if (!pipeline->enabled()) {
        // disabled pipelines don't need CRTCs
        pipeline->setCrtc(nullptr);
        return checkCrtcAssignment(connectors, crtcs);
    }
    DrmCrtc *currentCrtc = nullptr;
    if (m_atomicModeSetting) {
        // try the crtc that this connector is already connected to first
        uint32_t id = connector->getProp(DrmConnector::PropertyIndex::CrtcId)->pending();
        auto it = std::find_if(crtcs.begin(), crtcs.end(), [id](const auto &crtc) {
            return id == crtc->id();
        });
        if (it != crtcs.end()) {
            currentCrtc = *it;
            auto crtcsLeft = crtcs;
            crtcsLeft.removeOne(currentCrtc);
            pipeline->setCrtc(currentCrtc);
            do {
                DrmPipeline::Error err = checkCrtcAssignment(connectors, crtcsLeft);
                if (err == DrmPipeline::Error::None || err == DrmPipeline::Error::NoPermission || err == DrmPipeline::Error::FramePending) {
                    return err;
                }
            } while (pipeline->pruneModifier());
        }
    }
    for (const auto &crtc : qAsConst(crtcs)) {
        if (connector->isCrtcSupported(crtc) && crtc != currentCrtc) {
            auto crtcsLeft = crtcs;
            crtcsLeft.removeOne(crtc);
            pipeline->setCrtc(crtc);
            do {
                DrmPipeline::Error err = checkCrtcAssignment(connectors, crtcsLeft);
                if (err == DrmPipeline::Error::None || err == DrmPipeline::Error::NoPermission || err == DrmPipeline::Error::FramePending) {
                    return err;
                }
            } while (pipeline->pruneModifier());
        }
    }
    return DrmPipeline::Error::InvalidArguments;
}

DrmPipeline::Error DrmGpu::testPendingConfiguration()
{
    QVector<DrmConnector *> connectors;
    QVector<DrmCrtc *> crtcs;
    // only change resources that aren't currently leased away
    for (const auto &conn : m_connectors) {
        bool isLeased = std::any_of(m_drmOutputs.cbegin(), m_drmOutputs.cend(), [&conn](const auto output) {
            return output->lease() && output->pipeline()->connector() == conn.get();
        });
        if (!isLeased) {
            connectors.push_back(conn.get());
        }
    }
    for (const auto &crtc : m_crtcs) {
        bool isLeased = std::any_of(m_drmOutputs.cbegin(), m_drmOutputs.cend(), [&crtc](const auto output) {
            return output->lease() && output->pipeline()->crtc() == crtc.get();
        });
        if (!isLeased) {
            crtcs.push_back(crtc.get());
        }
    }
    if (m_atomicModeSetting) {
        // sort outputs by being already connected (to any CRTC) so that already working outputs get preferred
        std::sort(connectors.begin(), connectors.end(), [](auto c1, auto c2) {
            return c1->getProp(DrmConnector::PropertyIndex::CrtcId)->current() > c2->getProp(DrmConnector::PropertyIndex::CrtcId)->current();
        });
    }
    DrmPipeline::Error err = checkCrtcAssignment(connectors, crtcs);
    if (err == DrmPipeline::Error::None || err == DrmPipeline::Error::NoPermission || err == DrmPipeline::Error::FramePending) {
        return err;
    } else {
        // try again without hw rotation
        bool hwRotationUsed = false;
        for (const auto &pipeline : qAsConst(m_pipelines)) {
            hwRotationUsed |= (pipeline->bufferOrientation() != DrmPlane::Transformations(DrmPlane::Transformation::Rotate0));
            pipeline->setBufferOrientation(DrmPlane::Transformation::Rotate0);
        }
        if (hwRotationUsed) {
            err = checkCrtcAssignment(connectors, crtcs);
        }
        return err;
    }
}

DrmPipeline::Error DrmGpu::testPipelines()
{
    QVector<DrmPipeline *> inactivePipelines;
    std::copy_if(m_pipelines.constBegin(), m_pipelines.constEnd(), std::back_inserter(inactivePipelines), [](const auto pipeline) {
        return pipeline->enabled() && !pipeline->active();
    });
    DrmPipeline::Error test = DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::TestAllowModeset, unusedObjects());
    if (!inactivePipelines.isEmpty() && test == DrmPipeline::Error::None) {
        // ensure that pipelines that are set as enabled but currently inactive
        // still work when they need to be set active again
        for (const auto pipeline : qAsConst(inactivePipelines)) {
            pipeline->setActive(true);
        }
        test = DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::TestAllowModeset, unusedObjects());
        for (const auto pipeline : qAsConst(inactivePipelines)) {
            pipeline->setActive(false);
        }
    }
    return test;
}

DrmOutput *DrmGpu::findOutput(quint32 connector)
{
    auto it = std::find_if(m_drmOutputs.constBegin(), m_drmOutputs.constEnd(), [connector](DrmOutput *o) {
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
        const bool idle = std::all_of(m_drmOutputs.constBegin(), m_drmOutputs.constEnd(), [](DrmOutput *output) {
            return !output->pipeline()->pageflipPending();
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

void DrmGpu::pageFlipHandler(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, unsigned int crtc_id, void *user_data)
{
    Q_UNUSED(fd)
    Q_UNUSED(sequence)

    DrmGpu *gpu = static_cast<DrmGpu *>(user_data);

    // The static_cast<> here are for a 32-bit environment where
    // sizeof(time_t) == sizeof(unsigned int) == 4 . Putting @p sec
    // into a time_t cuts off the most-significant bit (after the
    // year 2038), similarly long can't hold all the bits of an
    // unsigned multiplication.
    std::chrono::nanoseconds timestamp = convertTimestamp(gpu->presentationClock(), CLOCK_MONOTONIC,
                                                          {static_cast<time_t>(sec), static_cast<long>(usec * 1000)});
    if (timestamp == std::chrono::nanoseconds::zero()) {
        qCDebug(KWIN_DRM, "Got invalid timestamp (sec: %u, usec: %u) on gpu %s",
                sec, usec, qPrintable(gpu->devNode()));
        timestamp = std::chrono::steady_clock::now().time_since_epoch();
    }
    const auto pipelines = gpu->pipelines();
    auto it = std::find_if(pipelines.begin(), pipelines.end(), [crtc_id](const auto &pipeline) {
        return pipeline->currentCrtc() && pipeline->currentCrtc()->id() == crtc_id;
    });
    if (it == pipelines.end()) {
        qCWarning(KWIN_DRM, "received invalid page flip event for crtc %u", crtc_id);
    } else {
        (*it)->pageFlipped(timestamp);
    }
}

void DrmGpu::dispatchEvents()
{
    if (!m_platform->isActive()) {
        return;
    }
    drmEventContext context = {};
    context.version = 3;
    context.page_flip_handler2 = pageFlipHandler;
    drmHandleEvent(m_fd, &context);
}

void DrmGpu::removeOutput(DrmOutput *output)
{
    qCDebug(KWIN_DRM) << "Removing output" << output;
    m_drmOutputs.removeOne(output);
    m_pipelines.removeOne(output->pipeline());
    output->pipeline()->setLayers(nullptr, nullptr);
    m_outputs.removeOne(output);
    Q_EMIT outputRemoved(output);
    delete output;
}

DrmBackend *DrmGpu::platform() const
{
    return m_platform;
}

const QVector<DrmPipeline *> DrmGpu::pipelines() const
{
    return m_pipelines;
}

DrmVirtualOutput *DrmGpu::createVirtualOutput(const QString &name, const QSize &size, double scale, DrmVirtualOutput::Type type)
{
    auto output = new DrmVirtualOutput(name, this, size, type);
    output->setScale(scale);
    m_outputs << output;
    Q_EMIT outputAdded(output);
    return output;
}

void DrmGpu::removeVirtualOutput(DrmVirtualOutput *output)
{
    if (m_outputs.removeOne(output)) {
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

void DrmGpu::handleLeaseRequest(KWaylandServer::DrmLeaseV1Interface *leaseRequest)
{
    QVector<uint32_t> objects;
    QVector<DrmOutput *> outputs;

    const auto connectors = leaseRequest->connectors();
    for (KWaylandServer::DrmLeaseConnectorV1Interface *connector : connectors) {
        if (DrmOutput *output = findOutput(connector->id())) {
            if (output->lease()) {
                continue; // already leased
            }
            if (!output->addLeaseObjects(objects)) {
                leaseRequest->deny();
                return;
            }
            outputs << output;
        }
    }

    uint32_t lesseeId;
    FileDescriptor fd{drmModeCreateLease(m_fd, objects.constData(), objects.count(), 0, &lesseeId)};
    if (!fd.isValid()) {
        qCWarning(KWIN_DRM) << "Could not create DRM lease!" << strerror(errno);
        qCWarning(KWIN_DRM, "Tried to lease the following %d resources:", objects.count());
        for (const auto &res : qAsConst(objects)) {
            qCWarning(KWIN_DRM) << res;
        }
        leaseRequest->deny();
    } else {
        qCDebug(KWIN_DRM, "Created lease for %d resources:", objects.count());
        for (const auto &res : qAsConst(objects)) {
            qCDebug(KWIN_DRM) << res;
        }
        leaseRequest->grant(std::move(fd), lesseeId);
        for (const auto &output : qAsConst(outputs)) {
            output->leased(leaseRequest);
        }
    }
}

void DrmGpu::handleLeaseRevoked(KWaylandServer::DrmLeaseV1Interface *lease)
{
    const auto connectors = lease->connectors();
    for (KWaylandServer::DrmLeaseConnectorV1Interface *connector : connectors) {
        if (DrmOutput *output = findOutput(connector->id())) {
            output->leaseEnded();
        }
    }
    qCDebug(KWIN_DRM, "Revoking lease with leaseID %d", lease->lesseeId());
    drmModeRevokeLease(m_fd, lease->lesseeId());
}

QVector<DrmAbstractOutput *> DrmGpu::outputs() const
{
    return m_outputs;
}

int DrmGpu::fd() const
{
    return m_fd;
}

dev_t DrmGpu::deviceId() const
{
    return m_deviceId;
}

bool DrmGpu::atomicModeSetting() const
{
    return m_atomicModeSetting;
}

QString DrmGpu::devNode() const
{
    return m_devNode;
}

gbm_device *DrmGpu::gbmDevice() const
{
    return m_gbmDevice;
}

EGLDisplay DrmGpu::eglDisplay() const
{
    return m_eglDisplay;
}

void DrmGpu::setEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

bool DrmGpu::addFB2ModifiersSupported() const
{
    return m_addFB2ModifiersSupported;
}

bool DrmGpu::isNVidia() const
{
    return m_isNVidia;
}

bool DrmGpu::needsModeset() const
{
    return std::any_of(m_pipelines.constBegin(), m_pipelines.constEnd(), [](const auto &pipeline) {
        return pipeline->needsModeset();
    });
}

bool DrmGpu::maybeModeset()
{
    auto pipelines = m_pipelines;
    for (const auto &output : qAsConst(m_drmOutputs)) {
        if (output->lease()) {
            pipelines.removeOne(output->pipeline());
        }
    }
    bool presentPendingForAll = std::all_of(pipelines.constBegin(), pipelines.constEnd(), [](const auto &pipeline) {
        return pipeline->modesetPresentPending() || !pipeline->activePending();
    });
    if (!presentPendingForAll) {
        // commit only once all pipelines are ready for presentation
        return true;
    }
    // make sure there's no pending pageflips
    waitIdle();
    const DrmPipeline::Error err = DrmPipeline::commitPipelines(pipelines, DrmPipeline::CommitMode::CommitModeset, unusedObjects());
    for (DrmPipeline *pipeline : qAsConst(pipelines)) {
        if (pipeline->modesetPresentPending()) {
            pipeline->resetModesetPresentPending();
            if (err != DrmPipeline::Error::None) {
                pipeline->output()->frameFailed();
            }
        }
    }
    if (err == DrmPipeline::Error::None) {
        return true;
    } else {
        if (err != DrmPipeline::Error::FramePending) {
            QTimer::singleShot(0, m_platform, &DrmBackend::updateOutputs);
        }
        return false;
    }
}

QVector<DrmObject *> DrmGpu::unusedObjects() const
{
    if (!m_atomicModeSetting) {
        return {};
    }
    QVector<DrmObject *> ret = m_allObjects;
    for (const auto &pipeline : m_pipelines) {
        ret.removeOne(pipeline->connector());
        if (pipeline->crtc()) {
            ret.removeOne(pipeline->crtc());
            ret.removeOne(pipeline->crtc()->primaryPlane());
            ret.removeOne(pipeline->crtc()->cursorPlane());
        }
    }
    return ret;
}

QSize DrmGpu::cursorSize() const
{
    return m_cursorSize;
}

void DrmGpu::releaseBuffers()
{
    for (const auto &plane : qAsConst(m_planes)) {
        plane->releaseBuffers();
    }
    for (const auto &crtc : qAsConst(m_crtcs)) {
        crtc->releaseBuffers();
    }
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->primaryLayer()->releaseBuffers();
        pipeline->cursorLayer()->releaseBuffers();
    }
    for (const auto &output : qAsConst(m_outputs)) {
        if (const auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output)) {
            virtualOutput->outputLayer()->releaseBuffers();
        }
    }
}

void DrmGpu::recreateSurfaces()
{
    for (const auto &pipeline : qAsConst(m_pipelines)) {
        pipeline->setLayers(m_platform->renderBackend()->createPrimaryLayer(pipeline), m_platform->renderBackend()->createCursorLayer(pipeline));
        pipeline->applyPendingChanges();
    }
    for (const auto &output : qAsConst(m_outputs)) {
        if (const auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output)) {
            virtualOutput->recreateSurface();
        }
    }
    for (const auto &output : qAsConst(m_drmOutputs)) {
        output->updateCursor();
    }
}

}
