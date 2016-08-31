/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
#endif
// KWayland
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/outputconfiguration_interface.h>
// KF5
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <QCryptographicHash>
#include <QSocketNotifier>
#include <QPainter>
// system
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

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : Platform(parent)
    , m_udev(new Udev)
    , m_udevMonitor(m_udev->monitor())
    , m_dpmsFilter()
{
    handleOutputs();
    m_cursor[0] = nullptr;
    m_cursor[1] = nullptr;
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
        qDeleteAll(m_outputs);
        delete m_cursor[0];
        delete m_cursor[1];
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
}

void DrmBackend::outputWentOff()
{
    if (!m_dpmsFilter.isNull()) {
        // already another output is off
        return;
    }
    m_dpmsFilter.reset(new DpmsInputEventFilter(this));
    input()->prepandInputEventFilter(m_dpmsFilter.data());
}

void DrmBackend::turnOutputsOn()
{
    m_dpmsFilter.reset();
    for (auto it = m_outputs.constBegin(), end = m_outputs.constEnd(); it != end; it++) {
        (*it)->setDpms(DrmOutput::DpmsMode::On);
    }
}

void DrmBackend::checkOutputsAreOn()
{
    if (m_dpmsFilter.isNull()) {
        // already disabled, all outputs are on
        return;
    }
    for (auto it = m_outputs.constBegin(), end = m_outputs.constEnd(); it != end; it++) {
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
        reactivate();
    } else {
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
        DrmBuffer *c = m_cursor[(m_cursorIndex + 1) % 2];
        const QPoint cp = Cursor::pos() - softwareCursorHotspot();
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            DrmOutput *o = *it;
            o->pageFlipped();
            o->blank();
            o->showCursor(c);
            o->moveCursor(cp);
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
        o->restoreSaved();
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
    int fd = LogindIntegration::self()->takeDevice(device->devNode());
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << device->devNode();
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
            e.version = DRM_EVENT_CONTEXT_VERSION;
            e.page_flip_handler = pageFlipHandler;
            drmHandleEvent(m_fd, &e);
        }
    );
    m_drmId = device->sysNum();

    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    if (qEnvironmentVariableIsSet("KWIN_DRM_AMS")) {
        if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting.";
            m_atomicModeSetting = true;

            ScopedDrmPointer<drmModePlaneRes, &drmModeFreePlaneResources> planeResources(drmModeGetPlaneResources(m_fd));
            if (!planeResources) {
                qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode";
                m_atomicModeSetting = false;
            }

            if (m_atomicModeSetting) {
                qCDebug(KWIN_DRM) << "Number of planes:" << planeResources->count_planes;

                // create the plane objects
                for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
                    drmModePlane *kplane = drmModeGetPlane(m_fd, planeResources->planes[i]);
                    DrmPlane *p = new DrmPlane(kplane->plane_id, m_fd);

                    if (p->init()) {
                        p->setPossibleCrtcs(kplane->possible_crtcs);
                        p->setFormats(kplane->formats, kplane->count_formats);
                        m_planes << p;
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

    queryResources();
    if (m_outputs.isEmpty()) {
        qCWarning(KWIN_DRM) << "No outputs, cannot render, will terminate now";
        emit initFailed();
        return;
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
                        queryResources();
                        m_cursorIndex = (m_cursorIndex + 1) % 2;
                        updateCursor();
                    }
                }
            );
            m_udevMonitor->enable();
        }
    }
    setReady(true);

    initCursor();
}

void DrmBackend::queryResources()
{
    if (m_fd < 0) {
        return;
    }
    ScopedDrmPointer<_drmModeRes, &drmModeFreeResources> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        return;
    }

    QVector<DrmOutput*> connectedOutputs;
    for (int i = 0; i < resources->count_connectors; ++i) {
        const auto id = resources->connectors[i];
        ScopedDrmPointer<_drmModeConnector, &drmModeFreeConnector> connector(drmModeGetConnector(m_fd, id));
        if (!connector) {
            continue;
        }
        if (connector->connection != DRM_MODE_CONNECTED) {
            continue;
        }
        if (connector->count_modes == 0) {
            continue;
        }
        if (DrmOutput *o = findOutput(connector->connector_id)) {
            connectedOutputs << o;
            continue;
        }
        bool crtcFound = false;
        const quint32 crtcId = findCrtc(resources.data(), connector.data(), &crtcFound);
        if (!crtcFound) {
            continue;
        }
        ScopedDrmPointer<_drmModeCrtc, &drmModeFreeCrtc> crtc(drmModeGetCrtc(m_fd, crtcId));
        if (!crtc) {
            continue;
        }
        DrmOutput *drmOutput = new DrmOutput(this);
        connect(drmOutput, &DrmOutput::dpmsChanged, this, &DrmBackend::outputDpmsChanged);
        drmOutput->m_crtcId = crtcId;
        drmOutput->m_connector = connector->connector_id;

        if (m_atomicModeSetting) {
            drmOutput->m_crtc = new DrmCrtc(crtcId, m_fd);
            if (drmOutput->m_crtc->init()) {
                drmOutput->m_crtc->setOutput(drmOutput);
            } else {
                qCWarning(KWIN_DRM) << "Crtc object failed, skipping output on connector" << connector->connector_id;
                delete drmOutput->m_crtc;
                delete drmOutput;
                continue;
            }

            drmOutput->m_conn = new DrmConnector(connector->connector_id, m_fd);
            if (drmOutput->m_conn->init()) {
                drmOutput->m_conn->setOutput(drmOutput);
            } else {
                qCWarning(KWIN_DRM) << "Connector object failed, skipping output on connector" << connector->connector_id;
                delete drmOutput->m_conn;
                delete drmOutput;
                continue;
            }
        }

        if (crtc->mode_valid) {
            drmOutput->m_mode = crtc->mode;
        } else {
            drmOutput->m_mode = connector->modes[0];
        }
        qCDebug(KWIN_DRM) << "For new output use mode " << drmOutput->m_mode.name;

        if (!drmOutput->init(connector.data())) {
            qCWarning(KWIN_DRM) << "Failed to create output for connector " << connector->connector_id;
            delete drmOutput;
            continue;
        }
        qCDebug(KWIN_DRM) << "Found new output with uuid" << drmOutput->uuid();
        connectedOutputs << drmOutput;
    }
    std::sort(connectedOutputs.begin(), connectedOutputs.end(), [] (DrmOutput *a, DrmOutput *b) { return a->m_connector < b->m_connector; });
    // check for outputs which got removed
    auto it = m_outputs.begin();
    while (it != m_outputs.end()) {
        if (connectedOutputs.contains(*it)) {
            it++;
            continue;
        }
        DrmOutput *removed = *it;
        it = m_outputs.erase(it);
        emit outputRemoved(removed);
        delete removed;
    }
    for (auto it = connectedOutputs.constBegin(); it != connectedOutputs.constEnd(); ++it) {
        if (!m_outputs.contains(*it)) {
            emit outputAdded(*it);
        }
    }
    m_outputs = connectedOutputs;
    readOutputsConfiguration();
    if (!m_outputs.isEmpty()) {
        emit screensQueried();
    }
}

void DrmBackend::readOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QByteArray uuid = generateOutputConfigurationUuid();
    const auto outputGroup = kwinApp()->config()->group("DrmOutputs");
    const auto configGroup = outputGroup.group(uuid);
    qCDebug(KWIN_DRM) << "Reading output configuration for" << uuid;
    // default position goes from left to right
    QPoint pos(0, 0);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        const auto outputConfig = configGroup.group((*it)->uuid());
        (*it)->setGlobalPos(outputConfig.readEntry<QPoint>("Position", pos));
        // TODO: add mode
        pos.setX(pos.x() + (*it)->size().width());
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

void DrmBackend::configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config)
{
    const auto changes = config->changes();
    for (auto it = changes.begin(); it != changes.end(); it++) {

        KWayland::Server::OutputChangeSet *changeset = it.value();

        auto drmoutput = findOutput(it.key()->uuid());
        if (drmoutput == nullptr) {
            qCWarning(KWIN_DRM) << "Could NOT find DrmOutput matching " << it.key()->uuid();
            return;
        }
        drmoutput->setChanges(changeset);
    }
    emit screens()->changed();
}

DrmOutput *DrmBackend::findOutput(quint32 connector)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [connector] (DrmOutput *o) {
        return o->m_connector == connector;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

DrmOutput *DrmBackend::findOutput(const QByteArray &uuid)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [uuid] (DrmOutput *o) {
        return o->m_uuid == uuid;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

quint32 DrmBackend::findCrtc(drmModeRes *res, drmModeConnector *connector, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    ScopedDrmPointer<_drmModeEncoder, &drmModeFreeEncoder> encoder(drmModeGetEncoder(m_fd, connector->encoder_id));
    if (encoder) {
        if (!crtcIsUsed(encoder->crtc_id)) {
            if (ok) {
                *ok = true;
            }
            return encoder->crtc_id;
        }
    }
    // let's iterate over all encoders to find a suitable crtc
    for (int i = 0; i < connector->count_encoders; ++i) {
        ScopedDrmPointer<_drmModeEncoder, &drmModeFreeEncoder> encoder(drmModeGetEncoder(m_fd, connector->encoders[i]));
        if (!encoder) {
            continue;
        }
        for (int j = 0; j < res->count_crtcs; ++j) {
            if (!(encoder->possible_crtcs & (1 << j))) {
                continue;
            }
            if (!crtcIsUsed(res->crtcs[j])) {
                if (ok) {
                    *ok = true;
                }
                return res->crtcs[j];
            }
        }
    }
    return 0;
}

bool DrmBackend::crtcIsUsed(quint32 crtc)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(),
        [crtc] (DrmOutput *o) {
            return o->m_crtcId == crtc;
        }
    );
    return it != m_outputs.constEnd();
}

void DrmBackend::present(DrmBuffer *buffer, DrmOutput *output)
{
    if (output->present(buffer)) {
        m_pageFlipsPending++;
        if (m_pageFlipsPending == 1 && Compositor::self()) {
            Compositor::self()->aboutToSwapBuffers();
        }
    }
}

void DrmBackend::initCursor()
{
    m_cursorEnabled = waylandServer()->seat()->hasPointer();
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::hasPointerChanged, this,
        [this] {
            m_cursorEnabled = waylandServer()->seat()->hasPointer();
            if (usesSoftwareCursor()) {
                return;
            }
            for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
                if (m_cursorEnabled) {
                    (*it)->showCursor(m_cursor[m_cursorIndex]);
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
    auto createCursor = [this, cursorSize] (int index) {
        m_cursor[index] = createBuffer(cursorSize);
        if (!m_cursor[index]->map(QImage::Format_ARGB32_Premultiplied)) {
            return false;
        }
        m_cursor[index]->image()->fill(Qt::transparent);
        return true;
    };
    if (!createCursor(0) || !createCursor(1)) {
        setSoftWareCursor(true);
        return;
    }
    // now we have screens and can set cursors, so start tracking
    connect(this, &DrmBackend::cursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursor::self(), &Cursor::posChanged, this, &DrmBackend::moveCursor);
}

void DrmBackend::setCursor()
{
    DrmBuffer *c = m_cursor[m_cursorIndex];
    m_cursorIndex = (m_cursorIndex + 1) % 2;
    if (m_cursorEnabled) {
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            (*it)->showCursor(c);
        }
    }
    markCursorAsRendered();
}

void DrmBackend::updateCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
    const QImage &cursorImage = softwareCursor();
    if (cursorImage.isNull()) {
        hideCursor();
        return;
    }
    QImage *c = m_cursor[m_cursorIndex]->image();
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();

    setCursor();
    moveCursor();
}

void DrmBackend::hideCursor()
{
    if (!m_cursorEnabled) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    const QPoint p = Cursor::pos() - softwareCursorHotspot();
    if (!m_cursorEnabled) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->moveCursor(p);
    }
}

QSize DrmBackend::size() const
{
    if (m_outputs.isEmpty()) {
        return QSize();
    }
    return m_outputs.first()->size();
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    return new DrmQPainterBackend(this);
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_GBM
    return new EglGbmBackend(this);
#else
    return Platform::createOpenGLBackend();
#endif
}

DrmBuffer *DrmBackend::createBuffer(const QSize &size)
{
    DrmBuffer *b = new DrmBuffer(this, size);
    m_buffers << b;
    return b;
}

DrmBuffer *DrmBackend::createBuffer(gbm_surface *surface)
{
#if HAVE_GBM
    DrmBuffer *b = new DrmBuffer(this, surface);
    b->m_deleteAfterPageFlip = true;
    m_buffers << b;
    return b;
#else
    return nullptr;
#endif
}

void DrmBackend::bufferDestroyed(DrmBuffer *b)
{
    m_buffers.removeAll(b);
}

void DrmBackend::outputDpmsChanged()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    bool enabled = false;
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        enabled = enabled || (*it)->isDpmsEnabled();
    }
    setOutputsEnabled(enabled);
}


}
