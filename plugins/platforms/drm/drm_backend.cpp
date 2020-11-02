/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "logind.h"
#include "main.h"
#include "scene_qpainter_drm_backend.h"
#include "screens_drm.h"
#include "udev.h"
#include "wayland_server.h"
#if HAVE_GBM
#include "egl_gbm_backend.h"
#include <gbm.h>
#include "gbm_dmabuf.h"
#endif
#if HAVE_EGL_STREAMS
#include "egl_stream_backend.h"
#endif
// KWayland
#include <KWaylandServer/seat_interface.h>
// KF5
#include <KConfigGroup>
#include <KCoreAddons>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <QCryptographicHash>
#include <QSocketNotifier>
#include <QPainter>
// system
#include <algorithm>
#include <unistd.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#define KWIN_DRM_EVENT_CONTEXT_VERSION 2

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : Platform(parent)
    , m_udev(new Udev)
    , m_udevMonitor(m_udev->monitor())
    , m_dpmsFilter()
{
    setSupportsGammaControl(true);
    supportsOutputChanges();
}

DrmBackend::~DrmBackend()
{
#if HAVE_GBM
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
#endif
    if (m_fd >= 0) {
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }

        qDeleteAll(m_planes);
        qDeleteAll(m_crtcs);
        qDeleteAll(m_connectors);
        close(m_fd);
    }
}

void DrmBackend::init()
{
    LogindIntegration *logind = LogindIntegration::self();
    auto takeControl = [logind, this]() {
        if (logind->hasSessionControl()) {
            openDrm();
        } else {
            logind->takeControl();
            connect(logind, &LogindIntegration::hasSessionControlChanged, this, &DrmBackend::openDrm);
        }
    };
    if (logind->isConnected()) {
        takeControl();
    } else {
        connect(logind, &LogindIntegration::connectedChanged, this, takeControl);
    }
    connect(logind, &LogindIntegration::prepareForSleep, this, [this] (bool active) {
        if (!active) {
            turnOutputsOn();
        }
    });
}

void DrmBackend::prepareShutdown()
{
    writeOutputsConfiguration();
    for (DrmOutput *output : m_outputs) {
        output->teardown();
    }
    Platform::prepareShutdown();
}

Outputs DrmBackend::outputs() const
{
    return m_outputs;
}

Outputs DrmBackend::enabledOutputs() const
{
    return m_enabledOutputs;
}

void DrmBackend::createDpmsFilter()
{
    if (!m_dpmsFilter.isNull()) {
        // already another output is off
        return;
    }
    m_dpmsFilter.reset(new DpmsInputEventFilter(this));
    input()->prependInputEventFilter(m_dpmsFilter.data());
}

void DrmBackend::turnOutputsOn()
{
    m_dpmsFilter.reset();
    for (auto it = m_enabledOutputs.constBegin(), end = m_enabledOutputs.constEnd(); it != end; it++) {
        (*it)->updateDpms(KWaylandServer::OutputInterface::DpmsMode::On);
    }
}

void DrmBackend::checkOutputsAreOn()
{
    if (m_dpmsFilter.isNull()) {
        // already disabled, all outputs are on
        return;
    }
    for (auto it = m_enabledOutputs.constBegin(), end = m_enabledOutputs.constEnd(); it != end; it++) {
        if (!(*it)->isDpmsEnabled()) {
            // dpms still disabled, need to keep the filter
            return;
        }
    }
    // all outputs are on, disable the filter
    m_dpmsFilter.reset();
}

void DrmBackend::activate(bool active)
{
    if (active) {
        qCDebug(KWIN_DRM) << "Activating session.";
        reactivate();
    } else {
        qCDebug(KWIN_DRM) << "Deactivating session.";
        deactivate();
    }
}

void DrmBackend::reactivate()
{
    if (m_active) {
        return;
    }
    m_active = true;
    if (!usesSoftwareCursor()) {
        Cursor* cursor = Cursors::self()->mouse();
        const QPoint cp = cursor->pos() - cursor->hotspot();
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            DrmOutput *o = *it;
            // only relevant in atomic mode
            o->m_modesetRequested = true;
            o->m_crtc->blank();
            o->showCursor();
            o->moveCursor(cursor, cp);
        }
    }
    // restart compositor
    m_pageFlipsPending = 0;
    if (Compositor *compositor = Compositor::self()) {
        compositor->bufferSwapComplete();
        compositor->addRepaintFull();
    }
}

void DrmBackend::deactivate()
{
    if (!m_active) {
        return;
    }
    // block compositor
    if (m_pageFlipsPending == 0 && Compositor::self()) {
        Compositor::self()->aboutToSwapBuffers();
    }
    // hide cursor and disable
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        DrmOutput *o = *it;
        o->hideCursor();
    }
    m_active = false;
}

void DrmBackend::pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    Q_UNUSED(fd)
    Q_UNUSED(frame)
    Q_UNUSED(sec)
    Q_UNUSED(usec)
    auto output = reinterpret_cast<DrmOutput*>(data);

    output->pageFlipped();
    output->m_backend->m_pageFlipsPending--;
    if (output->m_backend->m_pageFlipsPending == 0) {
        // TODO: improve, this currently means we wait for all page flips or all outputs.
        // It would be better to driver the repaint per output

        if (Compositor::self()) {
            Compositor::self()->bufferSwapComplete();
        }
    }
}

void DrmBackend::openDrm()
{
    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, this, &DrmBackend::activate);
    UdevDevice::Ptr device = m_udev->primaryGpu();
    if (!device) {
        qCWarning(KWIN_DRM) << "Did not find a GPU";
        return;
    }
    m_devNode = qEnvironmentVariableIsSet("KWIN_DRM_DEVICE_NODE") ? qgetenv("KWIN_DRM_DEVICE_NODE") : QByteArray(device->devNode());
    int fd = LogindIntegration::self()->takeDevice(m_devNode.constData());
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << m_devNode;
        return;
    }
    m_fd = fd;
    m_active = true;
    QSocketNotifier *notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this,
        [this] {
            if (!LogindIntegration::self()->isActiveSession()) {
                return;
            }
            drmEventContext e;
            memset(&e, 0, sizeof e);
            e.version = KWIN_DRM_EVENT_CONTEXT_VERSION;
            e.page_flip_handler = pageFlipHandler;
            drmHandleEvent(m_fd, &e);
        }
    );
    m_drmId = device->sysNum();

#if HAVE_EGL_STREAMS
    if (qEnvironmentVariableIsSet("KWIN_DRM_USE_EGL_STREAMS")) {
        m_useEglStreams = true;
    } else {
        // If KWIN_DRM_USE_EGL_STREAMS is not set and we know that we are running with
        // the nvidia proprietary driver, enable the EGLStreams backend anyway.
        DrmScopedPointer<drmVersion> version(drmGetVersion(fd));
        m_useEglStreams = version->name == QByteArrayLiteral("nvidia-drm");
    }
#endif

    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS")) {
        if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting.";
            m_atomicModeSetting = true;

            DrmScopedPointer<drmModePlaneRes> planeResources(drmModeGetPlaneResources(m_fd));
            if (!planeResources) {
                qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode";
                m_atomicModeSetting = false;
            }

            if (m_atomicModeSetting) {
                qCDebug(KWIN_DRM) << "Number of planes:" << planeResources->count_planes;

                // create the plane objects
                for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
                    DrmScopedPointer<drmModePlane> kplane(drmModeGetPlane(m_fd, planeResources->planes[i]));
                    DrmPlane *p = new DrmPlane(kplane->plane_id, m_fd);
                    if (p->atomicInit()) {
                        m_planes << p;
                        if (p->type() == DrmPlane::TypeIndex::Overlay) {
                            m_overlayPlanes << p;
                        }
                    } else {
                        delete p;
                    }
                }

                if (m_planes.isEmpty()) {
                    qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode";
                    m_atomicModeSetting = false;
                }
            }
        } else {
            qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode.";
        }
    }

    initCursor();
    if (!updateOutputs())
        return;

    if (m_outputs.isEmpty()) {
        qCDebug(KWIN_DRM) << "No connected outputs found on startup.";
    }

    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this,
                [this] {
                    auto device = m_udevMonitor->getDevice();
                    if (!device) {
                        return;
                    }
                    if (device->sysNum() != m_drmId) {
                        return;
                    }
                    if (device->hasProperty("HOTPLUG", "1")) {
                        qCDebug(KWIN_DRM) << "Received hot plug event for monitored drm device";
                        updateOutputs();
                        updateCursor();
                    }
                }
            );
            m_udevMonitor->enable();
        }
    }
    setReady(true);
}

bool DrmBackend::updateOutputs()
{
    if (m_fd < 0) {
        return false;
    }

    DrmScopedPointer<drmModeRes> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return false;
    }

    auto oldConnectors = m_connectors;
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

    auto oldCrtcs = m_crtcs;
    for (int i = 0; i < resources->count_crtcs; ++i) {
        const uint32_t currentCrtc = resources->crtcs[i];
        auto it = std::find_if(m_crtcs.constBegin(), m_crtcs.constEnd(), [currentCrtc] (DrmCrtc *c) { return c->id() == currentCrtc; });
        if (it == m_crtcs.constEnd()) {
            auto c = new DrmCrtc(currentCrtc, this, i);
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
        m_enabledOutputs.removeOne(removed);
        emit outputRemoved(removed);
        removedOutputs.append(removed);
    }

    // now check new connections
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

                DrmOutput *output = new DrmOutput(this);
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
                if (!output->initCursor(m_cursorSize)) {
                    setSoftWareCursor(true);
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
    m_enabledOutputs = connectedOutputs;
    readOutputsConfiguration();
    updateOutputsEnabled();
    if (!m_outputs.isEmpty()) {
        emit screensQueried();
    }

    for(DrmOutput* removedOutput : removedOutputs) {
        removedOutput->teardown();
        removedOutput->m_crtc = nullptr;
        removedOutput->m_conn = nullptr;
    }
    qDeleteAll(oldConnectors);
    qDeleteAll(oldCrtcs);
    return true;
}

void DrmBackend::readOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QByteArray uuid = generateOutputConfigurationUuid();
    const auto outputGroup = kwinApp()->config()->group("DrmOutputs");
    const auto configGroup = outputGroup.group(uuid);
    // default position goes from left to right
    QPoint pos(0, 0);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        qCDebug(KWIN_DRM) << "Reading output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        const auto outputConfig = configGroup.group((*it)->uuid());
        (*it)->setGlobalPos(outputConfig.readEntry<QPoint>("Position", pos));
        // TODO: add mode
        if (outputConfig.hasKey("Scale"))
            (*it)->setScale(outputConfig.readEntry("Scale", 1.0));
        pos.setX(pos.x() + (*it)->geometry().width());
    }
}

void DrmBackend::writeOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QByteArray uuid = generateOutputConfigurationUuid();
    auto configGroup = KSharedConfig::openConfig()->group("DrmOutputs").group(uuid);
    // default position goes from left to right
    for (auto it = m_outputs.cbegin(); it != m_outputs.cend(); ++it) {
        qCDebug(KWIN_DRM) << "Writing output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        auto outputConfig = configGroup.group((*it)->uuid());
        outputConfig.writeEntry("Scale", (*it)->scale());
    }
}

QByteArray DrmBackend::generateOutputConfigurationUuid() const
{
    auto it = m_outputs.constBegin();
    if (m_outputs.size() == 1) {
        // special case: one output
        return (*it)->uuid();
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    for (; it != m_outputs.constEnd(); ++it) {
        hash.addData((*it)->uuid());
    }
    return hash.result().toHex().left(10);
}

void DrmBackend::enableOutput(DrmOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_enabledOutputs.contains(output));
        m_enabledOutputs << output;
        emit outputAdded(output);
    } else {
        Q_ASSERT(m_enabledOutputs.contains(output));
        m_enabledOutputs.removeOne(output);
        Q_ASSERT(!m_enabledOutputs.contains(output));
        emit outputRemoved(output);
    }
    updateOutputsEnabled();
    checkOutputsAreOn();
    emit screensQueried();
}

DrmOutput *DrmBackend::findOutput(quint32 connector)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [connector] (DrmOutput *o) {
        return o->m_conn->id() == connector;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

bool DrmBackend::present(DrmBuffer *buffer, DrmOutput *output)
{
    if (!buffer || buffer->bufferId() == 0) {
        if (m_deleteBufferAfterPageFlip) {
            delete buffer;
        }
        return false;
    }

    if (output->present(buffer)) {
        m_pageFlipsPending++;
        if (m_pageFlipsPending == 1 && Compositor::self()) {
            Compositor::self()->aboutToSwapBuffers();
        }
        return true;
    } else if (m_deleteBufferAfterPageFlip) {
        delete buffer;
    }
    return false;
}

void DrmBackend::initCursor()
{

#if HAVE_EGL_STREAMS
    // Hardware cursors aren't currently supported with EGLStream backend,
    // possibly an NVIDIA driver bug
    if (m_useEglStreams) {
        setSoftWareCursor(true);
    }
#endif

    m_cursorEnabled = waylandServer()->seat()->hasPointer();
    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::hasPointerChanged, this,
        [this] {
            m_cursorEnabled = waylandServer()->seat()->hasPointer();
            if (usesSoftwareCursor()) {
                return;
            }
            for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
                if (m_cursorEnabled) {
                    if (!(*it)->showCursor()) {
                        setSoftWareCursor(true);
                    }
                } else {
                    (*it)->hideCursor();
                }
            }
        }
    );
    uint64_t capability = 0;
    QSize cursorSize;
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_WIDTH, &capability) == 0) {
        cursorSize.setWidth(capability);
    } else {
        cursorSize.setWidth(64);
    }
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_HEIGHT, &capability) == 0) {
        cursorSize.setHeight(capability);
    } else {
        cursorSize.setHeight(64);
    }
    m_cursorSize = cursorSize;
    // now we have screens and can set cursors, so start tracking
    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &DrmBackend::moveCursor);
}

void DrmBackend::setCursor()
{
    if (m_cursorEnabled) {
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            if (!(*it)->showCursor()) {
                setSoftWareCursor(true);
            }
        }
    }
}

void DrmBackend::updateCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
    if (isCursorHidden()) {
        return;
    }

    auto cursor = Cursors::self()->currentCursor();
    const QImage &cursorImage = cursor->image();
    if (cursorImage.isNull()) {
        doHideCursor();
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->updateCursor();
    }

    setCursor();

    moveCursor(cursor, cursor->pos());
}

void DrmBackend::doShowCursor()
{
    updateCursor();
}

void DrmBackend::doHideCursor()
{
    if (!m_cursorEnabled || usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor(Cursor *cursor, const QPoint &pos)
{
    if (!m_cursorEnabled || isCursorHidden() || usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->moveCursor(cursor, pos);
    }
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    m_deleteBufferAfterPageFlip = false;
    return new DrmQPainterBackend(this);
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_EGL_STREAMS
    if (m_useEglStreams) {
        m_deleteBufferAfterPageFlip = false;
        return new EglStreamBackend(this);
    }
#endif

#if HAVE_GBM
    m_deleteBufferAfterPageFlip = true;
    return new EglGbmBackend(this);
#else
    return Platform::createOpenGLBackend();
#endif
}

DrmDumbBuffer *DrmBackend::createBuffer(const QSize &size)
{
    DrmDumbBuffer *b = new DrmDumbBuffer(m_fd, size);
    return b;
}

#if HAVE_GBM
DrmSurfaceBuffer *DrmBackend::createBuffer(const std::shared_ptr<GbmSurface> &surface)
{
    DrmSurfaceBuffer *b = new DrmSurfaceBuffer(m_fd, surface);
    return b;
}
#endif

void DrmBackend::updateOutputsEnabled()
{
    bool enabled = false;
    for (auto it = m_enabledOutputs.constBegin(); it != m_enabledOutputs.constEnd(); ++it) {
        enabled = enabled || (*it)->isDpmsEnabled();
    }
    setOutputsEnabled(enabled);
}

QVector<CompositingType> DrmBackend::supportedCompositors() const
{
    if (selectedCompositor() != NoCompositing) {
        return {selectedCompositor()};
    }
#if HAVE_GBM
    return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
#elif HAVE_EGL_STREAMS
    return m_useEglStreams ?
        QVector<CompositingType>{OpenGLCompositing, QPainterCompositing} :
        QVector<CompositingType>{QPainterCompositing};
#else
    return QVector<CompositingType>{QPainterCompositing};
#endif
}

QString DrmBackend::supportInformation() const
{
    QString supportInfo;
    QDebug s(&supportInfo);
    s.nospace();
    s << "Name: " << "DRM" << Qt::endl;
    s << "Active: " << m_active << Qt::endl;
    s << "Atomic Mode Setting: " << m_atomicModeSetting << Qt::endl;
#if HAVE_EGL_STREAMS
    s << "Using EGL Streams: " << m_useEglStreams << Qt::endl;
#endif
    return supportInfo;
}

DmaBufTexture *DrmBackend::createDmaBufTexture(const QSize &size)
{
#if HAVE_GBM
    return GbmDmaBuf::createBuffer(size, m_gbmDevice);
#else
    return nullptr;
#endif
}

}
