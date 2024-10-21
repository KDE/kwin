/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_gpu.h"

#include "config-kwin.h"

#include "core/gbmgraphicsbufferallocator.h"
#include "core/session.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_commit_thread.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_plane.h"
// system
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <ranges>
#include <unistd.h>
// drm
#include <drm_fourcc.h>
#include <gbm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifndef DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT
#define DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT 6
#endif
#ifndef DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP
#define DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP 0x15
#endif

namespace KWin
{

DrmGpu::DrmGpu(DrmBackend *backend, int fd, std::unique_ptr<DrmDevice> &&device)
    : m_fd(fd)
    , m_drmDevice(std::move(device))
    , m_atomicModeSetting(false)
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
    qCDebug(KWIN_DRM) << "drmModeAddFB2WithModifiers is" << (m_addFB2ModifiersSupported ? "supported" : "not supported") << "on GPU" << this;

    // find out what driver this kms device is using
    DrmUniquePtr<drmVersion> version(drmGetVersion(fd));
    m_isI915 = strstr(version->name, "i915");
    m_isNVidia = strstr(version->name, "nvidia-drm");
    m_isAmdgpu = strstr(version->name, "amdgpu");
    m_isVmwgfx = strstr(version->name, "vmwgfx");
    m_isVirtualMachine = strstr(version->name, "virtio") || strstr(version->name, "qxl")
        || strstr(version->name, "vmwgfx") || strstr(version->name, "vboxvideo");

    m_socketNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
    connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &DrmGpu::dispatchEvents);

    initDrmResources();

    if (m_atomicModeSetting == false) {
        m_asyncPageflipSupported = drmGetCap(fd, DRM_CAP_ASYNC_PAGE_FLIP, &capability) == 0 && capability == 1;
    } else {
        m_asyncPageflipSupported = drmGetCap(fd, DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &capability) == 0 && capability == 1;
    }
}

DrmGpu::~DrmGpu()
{
    removeOutputs();
    m_eglDisplay.reset();
    m_crtcs.clear();
    m_connectors.clear();
    m_planes.clear();
    m_socketNotifier.reset();
    m_platform->session()->closeRestricted(m_fd);
}

FileDescriptor DrmGpu::createNonMasterFd() const
{
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
    if (noAMS) {
        qCWarning(KWIN_DRM) << "Atomic Mode Setting requested off via environment variable. Using legacy mode on GPU" << this;
    } else if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        if (m_isVirtualMachine) {
            // ATOMIC must be set before attemping CURSOR_PLANE_HOTSPOT
            if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT, 1) != 0) {
                qCWarning(KWIN_DRM, "Atomic Mode Setting disabled on GPU %s because of cursor offset issues in virtual machines", qPrintable(m_drmDevice->path()));
                drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 0);
                noAMS = true;
            }
        }
        DrmUniquePtr<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
        if (planeResources && !noAMS) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting on gpu" << this;
            qCDebug(KWIN_DRM) << "Number of planes on GPU" << this << ":" << planeResources->count_planes;
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
                qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode on GPU " << this;
            }
        } else {
            qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode on GPU " << this;
        }
    } else {
        qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode on GPU" << this;
    }

    m_atomicModeSetting = !m_planes.empty();

    DrmUniquePtr<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCCritical(KWIN_DRM) << "drmModeGetResources for getting CRTCs failed on GPU" << this;
        return;
    }
    QList<DrmPlane *> assignedPlanes;
    for (int i = 0; i < resources->count_crtcs; ++i) {
        uint32_t crtcId = resources->crtcs[i];
        QList<DrmPlane *> primaryCandidates;
        QList<DrmPlane *> cursorCandidates;
        for (const auto &plane : m_planes) {
            if (plane->isCrtcSupported(i) && !assignedPlanes.contains(plane.get())) {
                if (plane->type.enumValue() == DrmPlane::TypeIndex::Primary) {
                    primaryCandidates.push_back(plane.get());
                } else if (plane->type.enumValue() == DrmPlane::TypeIndex::Cursor) {
                    cursorCandidates.push_back(plane.get());
                }
            }
        }
        if (m_atomicModeSetting && primaryCandidates.empty()) {
            qCWarning(KWIN_DRM) << "Could not find a suitable primary plane for crtc" << resources->crtcs[i];
            continue;
        }
        const auto findBestPlane = [crtcId](const QList<DrmPlane *> &list) {
            // if the plane is already used with this crtc, prefer it
            const auto connected = std::ranges::find_if(list, [crtcId](DrmPlane *plane) {
                return plane->crtcId.value() == crtcId;
            });
            if (connected != list.end()) {
                return *connected;
            }
            // don't take away planes from other crtcs. The kernel currently rejects such commits
            const auto notconnected = std::ranges::find_if(list, [](DrmPlane *plane) {
                return plane->crtcId.value() == 0;
            });
            if (notconnected != list.end()) {
                return *notconnected;
            }
            return list.empty() ? nullptr : list.front();
        };
        DrmPlane *primary = findBestPlane(primaryCandidates);
        DrmPlane *cursor = findBestPlane(cursorCandidates);
        assignedPlanes.push_back(primary);
        if (cursor) {
            assignedPlanes.push_back(cursor);
        }
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
    if (!m_isActive) {
        return false;
    }
    waitIdle();
    DrmUniquePtr<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return false;
    }

    // In principle these things are supposed to be detected through the wayland protocol.
    // In practice SteamVR doesn't always behave correctly
    DrmUniquePtr<drmModeLesseeListRes> lessees{drmModeListLessees(m_fd)};
    for (const auto &output : std::as_const(m_drmOutputs)) {
        if (output->lease()) {
            const bool leaseActive = std::ranges::any_of(std::span(lessees->lessees, lessees->count), [output](uint32_t id) {
                return output->lease()->lesseeId() == id;
            });
            if (!leaseActive) {
                Q_EMIT output->lease()->revokeRequested();
            }
        }
    }

    // update crtc properties
    for (const auto &crtc : std::as_const(m_crtcs)) {
        crtc->updateProperties();
    }
    // update plane properties
    for (const auto &plane : std::as_const(m_planes)) {
        plane->updateProperties();
    }

    // check for added and removed connectors
    QList<DrmConnector *> existing;
    QList<DrmOutput *> addedOutputs;
    for (int i = 0; i < resources->count_connectors; ++i) {
        const uint32_t currentConnector = resources->connectors[i];
        const auto it = std::ranges::find_if(m_connectors, [currentConnector](const auto &connector) {
            return connector->id() == currentConnector;
        });
        if (it == m_connectors.end()) {
            auto conn = std::make_shared<DrmConnector>(this, currentConnector);
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
        } else if (!output) {
            qCDebug(KWIN_DRM, "New %soutput on GPU %s: %s", conn->isNonDesktop() ? "non-desktop " : "", qPrintable(m_drmDevice->path()), qPrintable(conn->modelName()));
            const auto pipeline = conn->pipeline();
            m_pipelines << pipeline;
            auto output = new DrmOutput(*it);
            m_drmOutputs << output;
            addedOutputs << output;
            Q_EMIT outputAdded(output);
            pipeline->setLayers(m_platform->renderBackend()->createDrmPlaneLayer(pipeline, DrmPlane::TypeIndex::Primary), m_platform->renderBackend()->createDrmPlaneLayer(pipeline, DrmPlane::TypeIndex::Cursor));
            pipeline->setActive(true);
            pipeline->setEnable(false);
            pipeline->applyPendingChanges();
        } else {
            output->updateConnectorProperties();
        }
        if (stillExists) {
            it++;
        } else {
            m_allObjects.removeOne(it->get());
            it = m_connectors.erase(it);
        }
    }

    // try to apply mode changes triggered above
    DrmPipeline::Error err = testPendingConfiguration();
    if (err == DrmPipeline::Error::None) {
        for (const auto &pipeline : std::as_const(m_pipelines)) {
            pipeline->applyPendingChanges();
        }
    } else {
        for (const auto &pipeline : std::as_const(m_pipelines)) {
            pipeline->revertPendingChanges();
        }
        // try again, with the mode changes reverted
        err = testPendingConfiguration();
    }
    if (err == DrmPipeline::Error::None) {
        for (const auto &pipeline : std::as_const(m_pipelines)) {
            pipeline->applyPendingChanges();
            if (pipeline->output() && !pipeline->crtc()) {
                pipeline->setEnable(false);
                pipeline->output()->updateEnabled(false);
            }
        }
    } else if (err == DrmPipeline::Error::NoPermission) {
        for (const auto &pipeline : std::as_const(m_pipelines)) {
            pipeline->revertPendingChanges();
        }
        for (const auto &output : std::as_const(addedOutputs)) {
            removeOutput(output);
            const auto it = std::ranges::find_if(m_connectors, [output](const auto &conn) {
                return conn.get() == output->connector();
            });
            Q_ASSERT(it != m_connectors.end());
            m_allObjects.removeOne(it->get());
            m_connectors.erase(it);
        }
    } else {
        qCWarning(KWIN_DRM, "Failed to find a working setup for new outputs!");
        for (const auto &pipeline : std::as_const(m_pipelines)) {
            pipeline->revertPendingChanges();
        }
        for (const auto &output : std::as_const(addedOutputs)) {
            output->updateEnabled(false);
            output->pipeline()->setEnable(false);
            output->pipeline()->applyPendingChanges();
        }
    }
    return true;
}

void DrmGpu::removeOutputs()
{
    const auto outputs = m_drmOutputs;
    for (const auto &output : outputs) {
        removeOutput(output);
    }
}

DrmPipeline::Error DrmGpu::checkCrtcAssignment(QList<DrmConnector *> connectors, const QList<DrmCrtc *> &crtcs)
{
    if (connectors.isEmpty() || crtcs.isEmpty()) {
        if (m_pipelines.isEmpty()) {
            // nothing to do
            return DrmPipeline::Error::None;
        }
        // remaining connectors can't be powered
        for (const auto &conn : std::as_const(connectors)) {
            qCWarning(KWIN_DRM) << "disabling connector" << conn->modelName() << "without a crtc";
            conn->pipeline()->setCrtc(nullptr);
        }
        return testPipelines();
    }
    auto connector = connectors.takeFirst();
    auto pipeline = connector->pipeline();
    if (!pipeline->enabled() || !connector->isConnected()) {
        // disabled pipelines don't need CRTCs
        pipeline->setCrtc(nullptr);
        return checkCrtcAssignment(connectors, crtcs);
    }
    DrmCrtc *currentCrtc = nullptr;
    if (m_atomicModeSetting) {
        // try the crtc that this connector is already connected to first
        const uint32_t id = connector->crtcId.value();
        auto it = std::ranges::find_if(crtcs, [id](const auto &crtc) {
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
    for (const auto &crtc : std::as_const(crtcs)) {
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
    QList<DrmConnector *> connectors;
    QList<DrmCrtc *> crtcs;
    // only change resources that aren't currently leased away
    for (const auto &conn : m_connectors) {
        const bool isLeased = std::ranges::any_of(m_drmOutputs, [&conn](const auto output) {
            return output->lease() && output->pipeline()->connector() == conn.get();
        });
        if (!isLeased) {
            connectors.push_back(conn.get());
        }
    }
    for (const auto &crtc : m_crtcs) {
        const bool isLeased = std::ranges::any_of(m_drmOutputs, [&crtc](const auto output) {
            return output->lease() && output->pipeline()->crtc() == crtc.get();
        });
        if (!isLeased) {
            crtcs.push_back(crtc.get());
        }
    }
    if (m_atomicModeSetting) {
        // sort outputs by being already connected (to any CRTC) so that already working outputs get preferred
        std::sort(connectors.begin(), connectors.end(), [](auto c1, auto c2) {
            return c1->crtcId.value() > c2->crtcId.value();
        });
    }
    // reset all outputs to their most basic configuration (primary plane without scaling)
    // for the test, and set the target rects appropriately
    for (const auto output : std::as_const(m_drmOutputs)) {
        if (!output->lease()) {
            const auto primary = output->primaryLayer();
            primary->setTargetRect(QRect(QPoint(0, 0), output->connector()->pipeline()->mode()->size()));
            primary->setSourceRect(QRect(QPoint(0, 0), output->connector()->pipeline()->mode()->size()));
            primary->setEnabled(true);
            output->cursorLayer()->setEnabled(false);
        }
    }
    return checkCrtcAssignment(connectors, crtcs);
}

DrmPipeline::Error DrmGpu::testPipelines()
{
    QList<DrmPipeline *> inactivePipelines;
    std::copy_if(m_pipelines.constBegin(), m_pipelines.constEnd(), std::back_inserter(inactivePipelines), [](const auto pipeline) {
        return pipeline->enabled() && !pipeline->active();
    });
    DrmPipeline::Error test = DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::TestAllowModeset, unusedObjects());
    if (!inactivePipelines.isEmpty() && test == DrmPipeline::Error::None) {
        // ensure that pipelines that are set as enabled but currently inactive
        // still work when they need to be set active again
        for (const auto pipeline : std::as_const(inactivePipelines)) {
            pipeline->setActive(true);
        }
        test = DrmPipeline::commitPipelines(m_pipelines, DrmPipeline::CommitMode::TestAllowModeset, unusedObjects());
        for (const auto pipeline : std::as_const(inactivePipelines)) {
            pipeline->setActive(false);
        }
    }
    return test;
}

DrmOutput *DrmGpu::findOutput(quint32 connector)
{
    auto it = std::ranges::find_if(m_drmOutputs, [connector](DrmOutput *o) {
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
        const bool hasPendingCommit = std::ranges::any_of(m_pipelines, [](DrmPipeline *pipeline) {
            return pipeline->commitThread()->drain();
        });
        if (!hasPendingCommit) {
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
            qCWarning(KWIN_DRM) << "No drm events for gpu" << this << "within last 30 seconds";
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
    const auto commit = static_cast<DrmCommit *>(user_data);
    const auto gpu = commit->gpu();

    // The static_cast<> here are for a 32-bit environment where
    // sizeof(time_t) == sizeof(unsigned int) == 4 . Putting @p sec
    // into a time_t cuts off the most-significant bit (after the
    // year 2038), similarly long can't hold all the bits of an
    // unsigned multiplication.
    std::chrono::nanoseconds timestamp = convertTimestamp(gpu->presentationClock(), CLOCK_MONOTONIC,
                                                          {static_cast<time_t>(sec), static_cast<long>(usec * 1000)});
    if (timestamp == std::chrono::nanoseconds::zero()) {
        qCDebug(KWIN_DRM, "Got invalid timestamp (sec: %u, usec: %u) on gpu %s",
                sec, usec, qPrintable(gpu->drmDevice()->path()));
        timestamp = std::chrono::steady_clock::now().time_since_epoch();
    }
    commit->pageFlipped(timestamp);
}

void DrmGpu::dispatchEvents()
{
    drmEventContext context = {};
    context.version = 3;
    context.page_flip_handler2 = pageFlipHandler;
    drmHandleEvent(m_fd, &context);
}

void DrmGpu::removeOutput(DrmOutput *output)
{
    qCDebug(KWIN_DRM) << "Removing output" << output;
    m_pipelines.removeOne(output->pipeline());
    output->pipeline()->setLayers(nullptr, nullptr);
    m_drmOutputs.removeOne(output);
    Q_EMIT outputRemoved(output);
    output->unref();
    // force a modeset to make sure unused objects are cleaned up
    m_forceModeset = true;
}

DrmBackend *DrmGpu::platform() const
{
    return m_platform;
}

const QList<DrmPipeline *> DrmGpu::pipelines() const
{
    return m_pipelines;
}

std::unique_ptr<DrmLease> DrmGpu::leaseOutputs(const QList<DrmOutput *> &outputs)
{
    const bool alreadyLeased = std::ranges::any_of(outputs, [](DrmOutput *output) {
        return output->lease();
    });
    if (alreadyLeased) {
        return nullptr;
    }

    // allocate crtcs for the outputss
    for (DrmOutput *output : outputs) {
        output->pipeline()->setEnable(true);
        output->pipeline()->setActive(false);
    }
    if (testPendingConfiguration() != DrmPipeline::Error::None) {
        return nullptr;
    }

    QList<uint32_t> objects;
    for (DrmOutput *output : outputs) {
        if (!output->addLeaseObjects(objects)) {
            return nullptr;
        }
    }

    uint32_t lesseeId;
    FileDescriptor fd{drmModeCreateLease(m_fd, objects.constData(), objects.count(), 0, &lesseeId)};
    if (!fd.isValid()) {
        qCWarning(KWIN_DRM) << "Could not create DRM lease!" << strerror(errno);
        qCWarning(KWIN_DRM) << "Tried to lease the following" << objects.count() << "resources:";
        for (const auto &res : std::as_const(objects)) {
            qCWarning(KWIN_DRM) << res;
        }
        return nullptr;
    } else {
        qCDebug(KWIN_DRM) << "Created lease for" << objects.count() << "resources:";
        for (const auto &res : std::as_const(objects)) {
            qCDebug(KWIN_DRM) << res;
        }
        return std::make_unique<DrmLease>(this, std::move(fd), lesseeId, outputs);
    }
}

QList<DrmOutput *> DrmGpu::drmOutputs() const
{
    return m_drmOutputs;
}

int DrmGpu::fd() const
{
    return m_fd;
}

DrmDevice *DrmGpu::drmDevice() const
{
    return m_drmDevice.get();
}

bool DrmGpu::atomicModeSetting() const
{
    return m_atomicModeSetting;
}

EglDisplay *DrmGpu::eglDisplay() const
{
    return m_eglDisplay.get();
}

void DrmGpu::setEglDisplay(std::unique_ptr<EglDisplay> &&display)
{
    m_eglDisplay = std::move(display);
}

bool DrmGpu::addFB2ModifiersSupported() const
{
    return m_addFB2ModifiersSupported;
}

bool DrmGpu::asyncPageflipSupported() const
{
    return m_asyncPageflipSupported;
}

bool DrmGpu::isI915() const
{
    return m_isI915;
}

bool DrmGpu::isNVidia() const
{
    return m_isNVidia;
}

bool DrmGpu::isAmdgpu() const
{
    return m_isAmdgpu;
}

bool DrmGpu::isVmwgfx() const
{
    return m_isVmwgfx;
}

bool DrmGpu::isVirtualMachine() const
{
    return m_isVirtualMachine;
}

bool DrmGpu::isRemoved() const
{
    return m_isRemoved;
}

void DrmGpu::setRemoved()
{
    m_isRemoved = true;
}

void DrmGpu::setActive(bool active)
{
    if (m_isActive != active) {
        m_isActive = active;
        if (active) {
            for (const auto &output : std::as_const(m_drmOutputs)) {
                output->renderLoop()->uninhibit();
            }
            for (const auto &output : std::as_const(m_drmOutputs)) {
                // force a modeset with legacy, we can't reliably know if one is needed
                if (!atomicModeSetting()) {
                    output->pipeline()->forceLegacyModeset();
                }
            }
        } else {
            for (const auto &output : std::as_const(m_drmOutputs)) {
                output->renderLoop()->inhibit();
            }
        }
        Q_EMIT activeChanged(active);
    }
}

bool DrmGpu::isActive() const
{
    return m_isActive;
}

bool DrmGpu::needsModeset() const
{
    return std::any_of(m_pipelines.constBegin(), m_pipelines.constEnd(), [](const auto &pipeline) {
        return pipeline->needsModeset();
    }) || m_forceModeset || !m_pendingModesetFrames.empty();
}

bool DrmGpu::maybeModeset(const std::shared_ptr<OutputFrame> &frame)
{
    if (frame) {
        m_pendingModesetFrames.push_back(frame);
    }
    auto pipelines = m_pipelines;
    for (const auto &output : std::as_const(m_drmOutputs)) {
        if (output->lease()) {
            pipelines.removeOne(output->pipeline());
        }
    }
    const bool presentPendingForAll = std::ranges::all_of(pipelines, [](const auto &pipeline) {
        return pipeline->modesetPresentPending() || !pipeline->activePending();
    });
    if (!presentPendingForAll) {
        // commit only once all pipelines are ready for presentation
        return true;
    }
    // make sure there's no pending pageflips
    waitIdle();
    const DrmPipeline::Error err = DrmPipeline::commitPipelines(pipelines, DrmPipeline::CommitMode::CommitModeset, unusedObjects());
    for (DrmPipeline *pipeline : std::as_const(pipelines)) {
        if (pipeline->modesetPresentPending()) {
            pipeline->resetModesetPresentPending();
        }
    }
    m_forceModeset = false;
    if (err == DrmPipeline::Error::None) {
        for (const auto &frame : m_pendingModesetFrames) {
            frame->presented(std::chrono::steady_clock::now().time_since_epoch(), PresentationMode::VSync);
        }
        m_pendingModesetFrames.clear();
        return true;
    } else {
        if (err != DrmPipeline::Error::FramePending) {
            QTimer::singleShot(0, m_platform, &DrmBackend::updateOutputs);
        }
        m_pendingModesetFrames.clear();
        return false;
    }
}

QList<DrmObject *> DrmGpu::unusedObjects() const
{
    QList<DrmObject *> ret = m_allObjects;
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
    for (const auto &plane : std::as_const(m_planes)) {
        plane->releaseCurrentBuffer();
    }
    for (const auto &crtc : std::as_const(m_crtcs)) {
        crtc->releaseCurrentBuffer();
    }
    for (const auto &pipeline : std::as_const(m_pipelines)) {
        if (DrmPipelineLayer *layer = pipeline->primaryLayer()) {
            layer->releaseBuffers();
        }
        if (DrmPipelineLayer *layer = pipeline->cursorLayer()) {
            layer->releaseBuffers();
        }
    }
}

void DrmGpu::recreateSurfaces()
{
    for (const auto &pipeline : std::as_const(m_pipelines)) {
        pipeline->setLayers(m_platform->renderBackend()->createDrmPlaneLayer(pipeline, DrmPlane::TypeIndex::Primary), m_platform->renderBackend()->createDrmPlaneLayer(pipeline, DrmPlane::TypeIndex::Cursor));
        pipeline->applyPendingChanges();
    }
}

std::shared_ptr<DrmFramebuffer> DrmGpu::importBuffer(GraphicsBuffer *buffer, FileDescriptor &&readFence)
{
    const DmaBufAttributes *attributes = buffer->dmabufAttributes();
    if (Q_UNLIKELY(!attributes)) {
        return nullptr;
    }

    uint32_t handles[] = {0, 0, 0, 0};
    auto cleanup = qScopeGuard([this, &handles]() {
        for (int i = 0; i < 4; ++i) {
            if (handles[i] == 0) {
                continue;
            }
            bool closed = false;
            for (int j = 0; j < i; ++j) {
                if (handles[i] == handles[j]) {
                    closed = true;
                    break;
                }
            }
            if (closed) {
                continue;
            }
            drmCloseBufferHandle(m_fd, handles[i]);
        }
    });
    for (int i = 0; i < attributes->planeCount; ++i) {
        if (drmPrimeFDToHandle(m_fd, attributes->fd[i].get(), &handles[i]) != 0) {
            qCWarning(KWIN_DRM) << "drmPrimeFDToHandle() failed";
            return nullptr;
        }
    }

    uint32_t framebufferId = 0;
    int ret;
    if (addFB2ModifiersSupported() && attributes->modifier != DRM_FORMAT_MOD_INVALID) {
        uint64_t modifier[4] = {0, 0, 0, 0};
        for (int i = 0; i < attributes->planeCount; ++i) {
            modifier[i] = attributes->modifier;
        }
        ret = drmModeAddFB2WithModifiers(m_fd,
                                         attributes->width,
                                         attributes->height,
                                         attributes->format,
                                         handles,
                                         attributes->pitch.data(),
                                         attributes->offset.data(),
                                         modifier,
                                         &framebufferId,
                                         DRM_MODE_FB_MODIFIERS);
    } else {
        ret = drmModeAddFB2(m_fd,
                            attributes->width,
                            attributes->height,
                            attributes->format,
                            handles,
                            attributes->pitch.data(),
                            attributes->offset.data(),
                            &framebufferId,
                            0);
        if (ret == EOPNOTSUPP && attributes->planeCount == 1) {
            ret = drmModeAddFB(m_fd,
                               attributes->width,
                               attributes->height,
                               24, 32,
                               attributes->pitch[0],
                               handles[0],
                               &framebufferId);
        }
    }

    if (ret != 0) {
        return nullptr;
    }

    return std::make_shared<DrmFramebuffer>(this, framebufferId, buffer, std::move(readFence));
}

DrmLease::DrmLease(DrmGpu *gpu, FileDescriptor &&fd, uint32_t lesseeId, const QList<DrmOutput *> &outputs)
    : m_gpu(gpu)
    , m_fd(std::move(fd))
    , m_lesseeId(lesseeId)
    , m_outputs(outputs)
{
    for (const auto output : m_outputs) {
        output->leased(this);
    }
}

DrmLease::~DrmLease()
{
    qCDebug(KWIN_DRM, "Revoking lease with leaseID %d", m_lesseeId);
    drmModeRevokeLease(m_gpu->fd(), m_lesseeId);
    for (const auto &output : m_outputs) {
        output->leaseEnded();
        output->pipeline()->setEnable(false);
    }
}

FileDescriptor &DrmLease::fd()
{
    return m_fd;
}

uint32_t DrmLease::lesseeId() const
{
    return m_lesseeId;
}
}

QDebug &operator<<(QDebug &s, const KWin::DrmGpu *gpu)
{
    s << gpu->drmDevice()->path();
    return s;
}

#include "moc_drm_gpu.cpp"
