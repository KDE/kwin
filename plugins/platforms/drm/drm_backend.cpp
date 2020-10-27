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
#include <libdrm/drm_mode.h>

#include "drm_gpu.h"

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
    if (m_gpus.size() > 0) {
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
        qDeleteAll(m_gpus);
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
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            DrmOutput *o = *it;
            // only relevant in atomic mode
            o->m_modesetRequested = true;
            o->m_crtc->blank();
            o->showCursor();
            o->moveCursor();
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
    std::vector<UdevDevice::Ptr> devices = m_udev->listGPUs();
    if (devices.size() == 0) {
        qCWarning(KWIN_DRM) << "Did not find a GPU";
        return;
    }
    
    for (unsigned int gpu_index = 0; gpu_index < devices.size(); gpu_index++) {
        auto device = std::move(devices.at(gpu_index));
        auto devNode = QByteArray(device->devNode());
        int fd = LogindIntegration::self()->takeDevice(devNode.constData());
        if (fd < 0) {
            qCWarning(KWIN_DRM) << "failed to open drm device at" << devNode;
            return;
        }

        // try to make a simple drm get resource call, if it fails it is not useful for us
        drmModeRes *resources = drmModeGetResources(fd);
        if (!resources) {
            qCDebug(KWIN_DRM) << "Skipping KMS incapable drm device node at" << devNode;
            LogindIntegration::self()->releaseDevice(fd);
            continue;
        }
        drmModeFreeResources(resources);

        m_active = true;
        QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this,
            [fd] {
                if (!LogindIntegration::self()->isActiveSession()) {
                    return;
                }
                drmEventContext e;
                memset(&e, 0, sizeof e);
                e.version = KWIN_DRM_EVENT_CONTEXT_VERSION;
                e.page_flip_handler = pageFlipHandler;
                drmHandleEvent(fd, &e);
            }
        );
        DrmGpu *gpu = new DrmGpu(this, devNode, fd, device->sysNum());
        connect(gpu, &DrmGpu::outputAdded, this, &DrmBackend::addOutput);
        connect(gpu, &DrmGpu::outputRemoved, this, &DrmBackend::removeOutput);
        m_gpus.append(gpu);
        break;
    }
    
    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS")) {
        for (auto gpu : m_gpus)
            gpu->tryAMS();
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
                    bool drm = false;
                    for (auto gpu : m_gpus) {
                        if (gpu->drmId() == device->sysNum()) {
                            drm = true;
                            break;
                        }
                    }
                    if (!drm) {
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

void DrmBackend::addOutput(DrmOutput *o)
{
    m_outputs.append(o);
    m_enabledOutputs.append(o);
    emit o->gpu()->outputEnabled(o);
}

void DrmBackend::removeOutput(DrmOutput *o)
{
    emit o->gpu()->outputDisabled(o);
    m_outputs.removeOne(o);
    m_enabledOutputs.removeOne(o);
}

bool DrmBackend::updateOutputs()
{
    if (m_gpus.size() == 0) {
        return false;
    }
    for (auto gpu : m_gpus)
        gpu->updateOutputs();
    
    std::sort(m_outputs.begin(), m_outputs.end(), [] (DrmOutput *a, DrmOutput *b) { return a->m_conn->id() < b->m_conn->id(); });
    readOutputsConfiguration();
    updateOutputsEnabled();
    if (!m_outputs.isEmpty()) {
        emit screensQueried();
    }
    return true;
}

static QString transformToString(DrmOutput::Transform transform)
{
    switch (transform) {
    case DrmOutput::Transform::Normal:
        return QStringLiteral("normal");
    case DrmOutput::Transform::Rotated90:
        return QStringLiteral("rotate-90");
    case DrmOutput::Transform::Rotated180:
        return QStringLiteral("rotate-180");
    case DrmOutput::Transform::Rotated270:
        return QStringLiteral("rotate-270");
    case DrmOutput::Transform::Flipped:
        return QStringLiteral("flip");
    case DrmOutput::Transform::Flipped90:
        return QStringLiteral("flip-90");
    case DrmOutput::Transform::Flipped180:
        return QStringLiteral("flip-180");
    case DrmOutput::Transform::Flipped270:
        return QStringLiteral("flip-270");
    default:
        return QStringLiteral("normal");
    }
}

static DrmOutput::Transform stringToTransform(const QString &text)
{
    static const QHash<QString, DrmOutput::Transform> stringToTransform {
        { QStringLiteral("normal"),     DrmOutput::Transform::Normal },
        { QStringLiteral("rotate-90"),  DrmOutput::Transform::Rotated90 },
        { QStringLiteral("rotate-180"), DrmOutput::Transform::Rotated180 },
        { QStringLiteral("rotate-270"), DrmOutput::Transform::Rotated270 },
        { QStringLiteral("flip"),       DrmOutput::Transform::Flipped },
        { QStringLiteral("flip-90"),    DrmOutput::Transform::Flipped90 },
        { QStringLiteral("flip-180"),   DrmOutput::Transform::Flipped180 },
        { QStringLiteral("flip-270"),   DrmOutput::Transform::Flipped270 }
    };
    return stringToTransform.value(text, DrmOutput::Transform::Normal);
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
        (*it)->setTransform(stringToTransform(outputConfig.readEntry("Transform", "normal")));
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
        outputConfig.writeEntry("Transform", transformToString((*it)->transform()));
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
        emit output->gpu()->outputEnabled(output);
    } else {
        Q_ASSERT(m_enabledOutputs.contains(output));
        m_enabledOutputs.removeOne(output);
        Q_ASSERT(!m_enabledOutputs.contains(output));
        emit output->gpu()->outputDisabled(output);
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
        if (output->gpu()->deleteBufferAfterPageFlip()) {
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
    } else if (output->gpu()->deleteBufferAfterPageFlip()) {
        delete buffer;
    }
    return false;
}

void DrmBackend::initCursor()
{

#if HAVE_EGL_STREAMS
    // Hardware cursors aren't currently supported with EGLStream backend,
    // possibly an NVIDIA driver bug
    bool needsSoftwareCursor = false;
    for (auto gpu : qAsConst(m_gpus)) {
        if (gpu->useEglStreams()) {
            needsSoftwareCursor = true;
            break;
        }
    }
    setSoftWareCursor(needsSoftwareCursor);
#endif

    if (waylandServer()->seat()->hasPointer()) {
        // The cursor is visible by default, do nothing.
    } else {
        hideCursor();
    }

    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::hasPointerChanged, this,
        [this] {
            if (waylandServer()->seat()->hasPointer()) {
                showCursor();
            } else {
                hideCursor();
            }
        }
    );
    // now we have screens and can set cursors, so start tracking
    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &DrmBackend::moveCursor);
}

void DrmBackend::setCursor()
{
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        if (!(*it)->showCursor()) {
            setSoftWareCursor(true);
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

    moveCursor();
}

void DrmBackend::doShowCursor()
{
    updateCursor();
}

void DrmBackend::doHideCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    if (isCursorHidden() || usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->moveCursor();
    }
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    m_gpus.at(0)->setDeleteBufferAfterPageFlip(false);
    return new DrmQPainterBackend(this, m_gpus.at(0));
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_EGL_STREAMS
    if (m_gpus.at(0)->useEglStreams()) {
        return new EglStreamBackend(this, m_gpus.at(0));
    }
#endif

#if HAVE_GBM
    return new EglGbmBackend(this, m_gpus.at(0));
#else
    return Platform::createOpenGLBackend();
#endif
}

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
    return m_gpus.at(0)->useEglStreams() ?
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
    for (int g = 0; g < m_gpus.size(); g++) {
        s << "Atomic Mode Setting on GPU " << g << ": " << m_gpus.at(g)->atomicModeSetting() << Qt::endl;
    }
#if HAVE_EGL_STREAMS
    s << "Using EGL Streams: " << m_gpus.at(0)->useEglStreams() << Qt::endl;
#endif
    return supportInfo;
}

DmaBufTexture *DrmBackend::createDmaBufTexture(const QSize &size)
{
#if HAVE_GBM
    // gpu_index is a fixed 0 here
    // as the first GPU is assumed to always be the one used for scene rendering
    // and this function is only used for Pipewire
    return GbmDmaBuf::createBuffer(size, m_gpus.at(0)->gbmDevice());
#else
    return nullptr;
#endif
}

}
