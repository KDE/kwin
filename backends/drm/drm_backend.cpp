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
#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "logind.h"
#include "scene_qpainter_drm_backend.h"
#include "screens_drm.h"
#include "udev.h"
#include "virtual_terminal.h"
#include "wayland_server.h"
#if HAVE_GBM
#include "egl_gbm_backend.h"
#endif
// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
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
#include <sys/mman.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>
#if HAVE_GBM
#include <gbm.h>
#endif

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

namespace KWin
{

DpmsInputEventFilter::DpmsInputEventFilter(DrmBackend *backend)
    : InputEventFilter()
    , m_backend(backend)
{
}

DpmsInputEventFilter::~DpmsInputEventFilter() = default;

bool DpmsInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    notify();
    return true;
}

bool DpmsInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    notify();
    return true;
}

bool DpmsInputEventFilter::keyEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    notify();
    return true;
}

void DpmsInputEventFilter::notify()
{
    // queued to not modify the list of event filters while filtering
    QMetaObject::invokeMethod(m_backend, "turnOutputsOn", Qt::QueuedConnection);
}

DrmBackend::DrmBackend(QObject *parent)
    : AbstractBackend(parent)
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
    if (m_fd >= 0) {
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
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
    auto v = VirtualTerminal::create(this);
    connect(v, &VirtualTerminal::activeChanged, this, &DrmBackend::activate);
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
    DrmBuffer *c = m_cursor[(m_cursorIndex + 1) % 2];
    const QPoint cp = Cursor::pos() - softwareCursorHotspot();
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        DrmOutput *o = *it;
        o->pageFlipped();
        o->blank();
        o->showCursor(c);
        o->moveCursor(cp);
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
    VirtualTerminal::self()->init();
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
            if (!VirtualTerminal::self()->isActive()) {
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
    queryResources();

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
        if (crtc->mode_valid) {
            drmOutput->m_mode = crtc->mode;
        } else {
            drmOutput->m_mode = connector->modes[0];
        }
        drmOutput->m_connector = connector->connector_id;
        drmOutput->init(connector.data());
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
    emit screensQueried();
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

void DrmBackend::installCursorFromServer()
{
    updateCursorFromServer();
}

void DrmBackend::installCursorImage(Qt::CursorShape shape)
{
    updateCursorImage(shape);
}

void DrmBackend::initCursor()
{
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
    m_cursor[0] = createBuffer(cursorSize);
    m_cursor[0]->map(QImage::Format_ARGB32_Premultiplied);
    m_cursor[0]->image()->fill(Qt::transparent);
    m_cursor[1] = createBuffer(cursorSize);
    m_cursor[1]->map(QImage::Format_ARGB32_Premultiplied);
    m_cursor[0]->image()->fill(Qt::transparent);
    // now we have screens and can set cursors, so start tracking
    connect(this, &DrmBackend::cursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursor::self(), &Cursor::posChanged, this, &DrmBackend::moveCursor);
    installCursorImage(Qt::ArrowCursor);
}

void DrmBackend::setCursor()
{
    DrmBuffer *c = m_cursor[m_cursorIndex];
    m_cursorIndex = (m_cursorIndex + 1) % 2;
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->showCursor(c);
    }
}

void DrmBackend::updateCursor()
{
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
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    const QPoint p = Cursor::pos() - softwareCursorHotspot();
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
    return AbstractBackend::createOpenGLBackend();
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

DrmOutput::DrmOutput(DrmBackend *backend)
    : QObject()
    , m_backend(backend)
{
}

DrmOutput::~DrmOutput()
{
    hideCursor();
    cleanupBlackBuffer();
    delete m_waylandOutput.data();
}

void DrmOutput::hideCursor()
{
    drmModeSetCursor(m_backend->fd(), m_crtcId, 0, 0, 0);
}

void DrmOutput::restoreSaved()
{
    if (!m_savedCrtc.isNull()) {
        drmModeSetCrtc(m_backend->fd(), m_savedCrtc->crtc_id, m_savedCrtc->buffer_id,
                       m_savedCrtc->x, m_savedCrtc->y, &m_connector, 1, &m_savedCrtc->mode);
    }
}

void DrmOutput::showCursor(DrmBuffer *c)
{
    const QSize &s = c->size();
    drmModeSetCursor(m_backend->fd(), m_crtcId, c->handle(), s.width(), s.height());
}

void DrmOutput::moveCursor(const QPoint &globalPos)
{
    const QPoint p = globalPos - m_globalPos;
    drmModeMoveCursor(m_backend->fd(), m_crtcId, p.x(), p.y());
}

QSize DrmOutput::size() const
{
    return QSize(m_mode.hdisplay, m_mode.vdisplay);
}

QRect DrmOutput::geometry() const
{
    return QRect(m_globalPos, size());
}

bool DrmOutput::present(DrmBuffer *buffer)
{
    if (!buffer || buffer->bufferId() == 0) {
        return false;
    }
    if (!VirtualTerminal::self()->isActive()) {
        m_currentBuffer = buffer;
        return false;
    }
    if (m_dpmsMode != DpmsMode::On) {
        return false;
    }
    if (m_currentBuffer) {
        return false;
    }
    if (m_lastStride != buffer->stride() || m_lastGbm != buffer->isGbm()) {
        // need to set a new mode first
        if (!setMode(buffer)) {
            return false;
        }
    }
    const bool ok = drmModePageFlip(m_backend->fd(), m_crtcId, buffer->bufferId(), DRM_MODE_PAGE_FLIP_EVENT, this) == 0;
    if (ok) {
        m_currentBuffer = buffer;
    } else {
        qCWarning(KWIN_DRM) << "Page flip failed";
        buffer->releaseGbm();
    }
    return ok;
}

void DrmOutput::pageFlipped()
{
    if (!m_currentBuffer) {
        return;
    }
    m_currentBuffer->releaseGbm();
    m_currentBuffer = nullptr;
    cleanupBlackBuffer();
}

void DrmOutput::cleanupBlackBuffer()
{
    if (m_blackBuffer) {
        delete m_blackBuffer;
        m_blackBuffer = nullptr;
    }
}

static KWayland::Server::OutputInterface::DpmsMode toWaylandDpmsMode(DrmOutput::DpmsMode mode)
{
    using namespace KWayland::Server;
    switch (mode) {
    case DrmOutput::DpmsMode::On:
        return OutputInterface::DpmsMode::On;
    case DrmOutput::DpmsMode::Standby:
        return OutputInterface::DpmsMode::Standby;
    case DrmOutput::DpmsMode::Suspend:
        return OutputInterface::DpmsMode::Suspend;
    case DrmOutput::DpmsMode::Off:
        return OutputInterface::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

static DrmOutput::DpmsMode fromWaylandDpmsMode(KWayland::Server::OutputInterface::DpmsMode wlMode)
{
    using namespace KWayland::Server;
    switch (wlMode) {
    case OutputInterface::DpmsMode::On:
        return DrmOutput::DpmsMode::On;
    case OutputInterface::DpmsMode::Standby:
        return DrmOutput::DpmsMode::Standby;
    case OutputInterface::DpmsMode::Suspend:
        return DrmOutput::DpmsMode::Suspend;
    case OutputInterface::DpmsMode::Off:
        return DrmOutput::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

void DrmOutput::init(drmModeConnector *connector)
{
    initEdid(connector);
    initDpms(connector);
    initUuid();
    m_savedCrtc.reset(drmModeGetCrtc(m_backend->fd(), m_crtcId));
    blank();
    setDpms(DpmsMode::On);
    if (!m_waylandOutput.isNull()) {
        delete m_waylandOutput.data();
        m_waylandOutput.clear();
    }
    m_waylandOutput = waylandServer()->display()->createOutput();
    if (!m_edid.eisaId.isEmpty()) {
        m_waylandOutput->setManufacturer(QString::fromLatin1(m_edid.eisaId));
    } else {
        m_waylandOutput->setManufacturer(i18n("unknown"));
    }
    if (!m_edid.monitorName.isEmpty()) {
        QString model = QString::fromLatin1(m_edid.monitorName);
        if (!m_edid.serialNumber.isEmpty()) {
            model.append('/');
            model.append(QString::fromLatin1(m_edid.serialNumber));
        }
        m_waylandOutput->setModel(model);
    } else if (!m_edid.serialNumber.isEmpty()) {
        m_waylandOutput->setModel(QString::fromLatin1(m_edid.serialNumber));
    } else {
        m_waylandOutput->setModel(i18n("unknown"));
    }

    QSize physicalSize = !m_edid.physicalSize.isEmpty() ? m_edid.physicalSize : QSize(connector->mmWidth, connector->mmHeight);
    // the size might be completely borked. E.g. Samsung SyncMaster 2494HS reports 160x90 while in truth it's 520x292
    // as this information is used to calculate DPI info, it's going to result in everything being huge
    const QByteArray unknown = QByteArrayLiteral("unkown");
    KConfigGroup group = kwinApp()->config()->group("EdidOverwrite").group(m_edid.eisaId.isEmpty() ? unknown : m_edid.eisaId)
                                                       .group(m_edid.monitorName.isEmpty() ? unknown : m_edid.monitorName)
                                                       .group(m_edid.serialNumber.isEmpty() ? unknown : m_edid.serialNumber);
    if (group.hasKey("PhysicalSize")) {
        const QSize overwriteSize = group.readEntry("PhysicalSize", physicalSize);
        qCWarning(KWIN_DRM) << "Overwriting monitor physical size for" << m_edid.eisaId << "/" << m_edid.monitorName << "/" << m_edid.serialNumber << " from " << physicalSize << "to " << overwriteSize;
        physicalSize = overwriteSize;
    }
    m_waylandOutput->setPhysicalSize(physicalSize);

    // read in mode information
    for (int i = 0; i < connector->count_modes; ++i) {
        auto *m = &connector->modes[i];
        KWayland::Server::OutputInterface::ModeFlags flags;
        if (isCurrentMode(m)) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Current;
        }
        if (m->type & DRM_MODE_TYPE_PREFERRED) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Preferred;
        }

        // Calculate higher precision (mHz) refresh rate
        // logic based on Weston, see compositor-drm.c
        quint64 refreshRate = (m->clock * 1000000LL / m->htotal + m->vtotal / 2) / m->vtotal;
        if (m->flags & DRM_MODE_FLAG_INTERLACE) {
            refreshRate *= 2;
        }
        if (m->flags & DRM_MODE_FLAG_DBLSCAN) {
            refreshRate /= 2;
        }
        if (m->vscan > 1) {
            refreshRate /= m->vscan;
        }
        m_waylandOutput->addMode(QSize(m->hdisplay, m->vdisplay), flags, refreshRate);
    }

    // set dpms
    if (!m_dpms.isNull()) {
        m_waylandOutput->setDpmsSupported(true);
        m_waylandOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsMode));
        connect(m_waylandOutput.data(), &KWayland::Server::OutputInterface::dpmsModeRequested, this,
            [this] (KWayland::Server::OutputInterface::DpmsMode mode) {
                setDpms(fromWaylandDpmsMode(mode));
            }, Qt::QueuedConnection
        );
    }

    m_waylandOutput->create();
}

void DrmOutput::initUuid()
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QByteArray::number(m_connector));
    hash.addData(m_edid.eisaId);
    hash.addData(m_edid.monitorName);
    hash.addData(m_edid.serialNumber);
    m_uuid = hash.result().toHex().left(10);
}

bool DrmOutput::isCurrentMode(const drmModeModeInfo *mode) const
{
    return mode->clock       == m_mode.clock
        && mode->hdisplay    == m_mode.hdisplay
        && mode->hsync_start == m_mode.hsync_start
        && mode->hsync_end   == m_mode.hsync_end
        && mode->htotal      == m_mode.htotal
        && mode->hskew       == m_mode.hskew
        && mode->vdisplay    == m_mode.vdisplay
        && mode->vsync_start == m_mode.vsync_start
        && mode->vsync_end   == m_mode.vsync_end
        && mode->vtotal      == m_mode.vtotal
        && mode->vscan       == m_mode.vscan
        && mode->vrefresh    == m_mode.vrefresh
        && mode->flags       == m_mode.flags
        && mode->type        == m_mode.type
        && qstrcmp(mode->name, m_mode.name) == 0;
}

void DrmOutput::blank()
{
    if (!m_blackBuffer) {
        m_blackBuffer = m_backend->createBuffer(size());
        m_blackBuffer->map();
        m_blackBuffer->image()->fill(Qt::black);
    }
    setMode(m_blackBuffer);
}

bool DrmOutput::setMode(DrmBuffer *buffer)
{
    if (drmModeSetCrtc(m_backend->fd(), m_crtcId, buffer->bufferId(), 0, 0, &m_connector, 1, &m_mode) == 0) {
        m_lastStride = buffer->stride();
        m_lastGbm = buffer->isGbm();
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Mode setting failed";
        return false;
    }
}

static bool verifyEdidHeader(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    if (data[0] != 0x00) {
        return false;
    }
    for (int i = 1; i < 7; ++i) {
        if (data[i] != 0xFF) {
            return false;
        }
    }
    if (data[7] != 0x00) {
        return false;
    }
    return true;
}

static QByteArray extractEisaId(drmModePropertyBlobPtr edid)
{
    /*
     * From EDID standard section 3.4:
     * The ID Manufacturer Name field, shown in Table 3.5, contains a 2-byte representation of the monitor's
     * manufacturer. This is the same as the EISA ID. It is based on compressed ASCII, “0001=A” ... “11010=Z”.
     *
     * The table:
     * | Byte |        Bit                    |
     * |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
     * ----------------------------------------
     * |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
     * |      | * |    Character 1    | Char 2|
     * ----------------------------------------
     * |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
     * |      | Character2|      Character 3  |
     * ----------------------------------------
     **/
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x8;
    char id[4];
    if (data[offset] >> 7) {
        // bit at position 7 is not a 0
        return QByteArray();
    }
    // shift two bits to right, and with 7 right most bits
    id[0] = 'A' + ((data[offset] >> 2) & 0x1f) -1;
    // for first byte: take last two bits and shift them 3 to left (000xx000)
    // for second byte: shift 5 bits to right and take 3 right most bits (00000xxx)
    // or both together
    id[1] = 'A' + (((data[offset] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    // take five right most bits
    id[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    id[3] = '\0';
    return QByteArray(id);
}

static void extractMonitorDescriptorDescription(drmModePropertyBlobPtr blob, DrmOutput::Edid &edid)
{
    // see section 3.10.3
    const uint8_t *data = reinterpret_cast<uint8_t*>(blob->data);
    static const uint offset = 0x36;
    static const uint blockLength = 18;
    for (int i = 0; i < 5; ++i) {
        const uint co = offset + i * blockLength;
        // Flag = 0000h when block used as descriptor
        if (data[co] != 0) {
            continue;
        }
        if (data[co + 1] != 0) {
            continue;
        }
        // Reserved = 00h when block used as descriptor
        if (data[co + 2] != 0) {
            continue;
        }
        /*
         * FFh: Monitor Serial Number - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FEh: ASCII String - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FDh: Monitor range limits, binary coded
         * FCh: Monitor name, stored as ASCII, code page # 437
         * FBh: Descriptor contains additional color point data
         * FAh: Descriptor contains additional Standard Timing Identifications
         * F9h - 11h: Currently undefined
         * 10h: Dummy descriptor, used to indicate that the descriptor space is unused
         * 0Fh - 00h: Descriptor defined by manufacturer.
         */
        if (data[co + 3] == 0xfc && edid.monitorName.isEmpty()) {
            edid.monitorName = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
        if (data[co + 3] == 0xfe) {
            const QByteArray id = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
            if (!id.isEmpty()) {
                edid.eisaId = id;
            }
        }
        if (data[co + 3] == 0xff) {
            edid.serialNumber = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
    }
}

static QByteArray extractSerialNumber(drmModePropertyBlobPtr edid)
{
    // see section 3.4
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x0C;
    /*
     * The ID serial number is a 32-bit serial number used to differentiate between individual instances of the same model
     * of monitor. Its use is optional. When used, the bit order for this field follows that shown in Table 3.6. The EDID
     * structure Version 1 Revision 1 and later offer a way to represent the serial number of the monitor as an ASCII string
     * in a separate descriptor block.
     */
    uint32_t serialNumber = 0;
    serialNumber  = (uint32_t) data[offset + 0];
    serialNumber |= (uint32_t) data[offset + 1] << 8;
    serialNumber |= (uint32_t) data[offset + 2] << 16;
    serialNumber |= (uint32_t) data[offset + 3] << 24;
    if (serialNumber == 0) {
        return QByteArray();
    }
    return QByteArray::number(serialNumber);
}

static QSize extractPhysicalSize(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    return QSize(data[0x15], data[0x16]) * 10;
}

void DrmOutput::initEdid(drmModeConnector *connector)
{
    ScopedDrmPointer<_drmModePropertyBlob, &drmModeFreePropertyBlob> edid;
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if ((property->flags & DRM_MODE_PROP_BLOB) && qstrcmp(property->name, "EDID") == 0) {
            edid.reset(drmModeGetPropertyBlob(m_backend->fd(), connector->prop_values[i]));
        }
    }
    if (!edid) {
        return;
    }

    // for documentation see: http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
    if (edid->length < 128) {
        return;
    }
    if (!verifyEdidHeader(edid.data())) {
        return;
    }
    m_edid.eisaId = extractEisaId(edid.data());
    m_edid.serialNumber = extractSerialNumber(edid.data());

    // parse monitor descriptor description
    extractMonitorDescriptorDescription(edid.data(), m_edid);

    m_edid.physicalSize = extractPhysicalSize(edid.data());
}

void DrmOutput::initDpms(drmModeConnector *connector)
{
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if (qstrcmp(property->name, "DPMS") == 0) {
            m_dpms.swap(property);
            break;
        }
    }
}

void DrmOutput::setDpms(DrmOutput::DpmsMode mode)
{
    if (m_dpms.isNull()) {
        return;
    }
    if (mode == m_dpmsMode) {
        return;
    }
    if (drmModeConnectorSetProperty(m_backend->fd(), m_connector, m_dpms->prop_id, uint64_t(mode)) != 0) {
        qCWarning(KWIN_DRM) << "Setting DPMS failed";
        return;
    }
    m_dpmsMode = mode;
    if (m_waylandOutput) {
        m_waylandOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsMode));
    }
    emit dpmsChanged();
    if (m_dpmsMode != DpmsMode::On) {
        m_backend->outputWentOff();
    } else {
        m_backend->checkOutputsAreOn();
        blank();
        if (Compositor *compositor = Compositor::self()) {
            compositor->addRepaintFull();
        }
    }
}

QString DrmOutput::name() const
{
    if (!m_waylandOutput) {
        return i18n("unknown");
    }
    return QStringLiteral("%1 %2").arg(m_waylandOutput->manufacturer()).arg(m_waylandOutput->model());
}

int DrmOutput::currentRefreshRate() const
{
    if (!m_waylandOutput) {
        return 60000;
    }
    return m_waylandOutput->refreshRate();
}

void DrmOutput::setGlobalPos(const QPoint &pos)
{
    m_globalPos = pos;
    if (m_waylandOutput) {
        m_waylandOutput->setGlobalPosition(pos);
    }
}

DrmBuffer::DrmBuffer(DrmBackend *backend, const QSize &size)
    : m_backend(backend)
    , m_size(size)
{
    drm_mode_create_dumb createArgs;
    memset(&createArgs, 0, sizeof createArgs);
    createArgs.bpp = 32;
    createArgs.width = size.width();
    createArgs.height = size.height();
    if (drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_CREATE_DUMB, &createArgs) != 0) {
        return;
    }
    m_handle = createArgs.handle;
    m_bufferSize = createArgs.size;
    m_stride = createArgs.pitch;
    drmModeAddFB(m_backend->fd(), size.width(), size.height(), 24, 32,
                 m_stride, createArgs.handle, &m_bufferId);
}


#if HAVE_GBM
static void gbmCallback(gbm_bo *bo, void *data)
{
    DrmBackend *backend = reinterpret_cast<DrmBackend*>(data);
    const auto &buffers = backend->buffers();
    for (auto buffer: buffers) {
        if (buffer->gbm() == bo) {
            delete buffer;
            return;
        }
    }
}
#endif

DrmBuffer::DrmBuffer(DrmBackend *backend, gbm_surface *surface)
    : m_backend(backend)
    , m_surface(surface)
{
#if HAVE_GBM
    m_bo = gbm_surface_lock_front_buffer(surface);
    if (!m_bo) {
        qCWarning(KWIN_DRM) << "Locking front buffer failed";
        return;
    }
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    m_stride = gbm_bo_get_stride(m_bo);
    if (drmModeAddFB(m_backend->fd(), m_size.width(), m_size.height(), 24, 32, m_stride, gbm_bo_get_handle(m_bo).u32, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed";
    }
    gbm_bo_set_user_data(m_bo, m_backend, gbmCallback);
#endif
}

DrmBuffer::~DrmBuffer()
{
    m_backend->bufferDestroyed(this);
    delete m_image;
    if (m_memory) {
        munmap(m_memory, m_bufferSize);
    }
    if (m_bufferId) {
        drmModeRmFB(m_backend->fd(), m_bufferId);
    }
    if (m_handle) {
        drm_mode_destroy_dumb destroyArgs;
        destroyArgs.handle = m_handle;
        drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
    }
    releaseGbm();
}

bool DrmBuffer::map(QImage::Format format)
{
    if (!m_handle || !m_bufferId) {
        return false;
    }
    drm_mode_map_dumb mapArgs;
    memset(&mapArgs, 0, sizeof mapArgs);
    mapArgs.handle = m_handle;
    if (drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_MAP_DUMB, &mapArgs) != 0) {
        return false;
    }
    void *address = mmap(nullptr, m_bufferSize, PROT_WRITE, MAP_SHARED, m_backend->fd(), mapArgs.offset);
    if (address == MAP_FAILED) {
        return false;
    }
    m_memory = address;
    m_image = new QImage((uchar*)m_memory, m_size.width(), m_size.height(), m_stride, format);
    return !m_image->isNull();
}

void DrmBuffer::releaseGbm()
{
#if HAVE_GBM
    if (m_bo) {
        gbm_surface_release_buffer(m_surface, m_bo);
        m_bo = nullptr;
    }
#endif
}

}
