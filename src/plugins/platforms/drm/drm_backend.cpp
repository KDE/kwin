/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_backend.h"
#include <config-kwin.h>
#include "drm_output.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "main.h"
#include "renderloop.h"
#include "scene_qpainter_drm_backend.h"
#include "session.h"
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
// system
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
// drm
#include <xf86drm.h>
#include <libdrm/drm_mode.h>

#include "drm_gpu.h"
#include "egl_multi_backend.h"

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : Platform(parent)
    , m_udev(new Udev)
    , m_udevMonitor(m_udev->monitor())
    , m_session(Session::create(this))
    , m_explicitGpus(qEnvironmentVariable("KWIN_DRM_DEVICES").split(':', Qt::SkipEmptyParts))
    , m_dpmsFilter()
{
    setSupportsGammaControl(true);
    setPerScreenRenderingEnabled(true);
    supportsOutputChanges();
}

DrmBackend::~DrmBackend()
{
    writeOutputsConfiguration();
    qDeleteAll(m_gpus);
}

Session *DrmBackend::session() const
{
    return m_session;
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
        (*it)->setDpmsMode(AbstractWaylandOutput::DpmsMode::On);
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
            o->m_crtc->blank(o);
        }
    }

    for (DrmOutput *output : qAsConst(m_outputs)) {
        output->renderLoop()->uninhibit();
    }
    if (Compositor *compositor = Compositor::self()) {
        compositor->addRepaintFull();
    }

    // While the session had been inactive, an output could have been added or
    // removed, we need to re-scan outputs.
    updateOutputs();
    updateCursor();
}

void DrmBackend::deactivate()
{
    if (!m_active) {
        return;
    }

    for (DrmOutput *output : qAsConst(m_outputs)) {
        output->hideCursor();
        output->renderLoop()->inhibit();
    }

    m_active = false;
}

bool DrmBackend::initialize()
{
    connect(session(), &Session::activeChanged, this, &DrmBackend::activate);
    connect(session(), &Session::awoke, this, &DrmBackend::turnOutputsOn);

    if (!m_explicitGpus.isEmpty()) {
        for (const QString &fileName : m_explicitGpus) {
            addGpu(fileName);
        }
    } else {
        const auto devices = m_udev->listGPUs();
        bool bootVga = false;
        for (const UdevDevice::Ptr &device : devices) {
            if (addGpu(device->devNode())) {
                bootVga |= device->isBootVga();
            }
        }

        // if a boot device is set, honor that setting
        // if not, prefer gbm for rendering because that works better
        if (!bootVga && !m_gpus.isEmpty() && m_gpus[0]->useEglStreams()) {
            for (int i = 1; i < m_gpus.count(); i++) {
                if (!m_gpus[i]->useEglStreams()) {
                    m_gpus.swapItemsAt(i, 0);
                    break;
                }
            }
        }
    }

    if (m_gpus.isEmpty()) {
        qCWarning(KWIN_DRM) << "No suitable DRM devices have been found";
        return false;
    }

    initCursor();
    if (!updateOutputs())
        return false;

    if (m_outputs.isEmpty()) {
        qCDebug(KWIN_DRM) << "No connected outputs found on startup.";
    }

    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this, &DrmBackend::handleUdevEvent);
            m_udevMonitor->enable();
        }
    }
    setReady(true);
    return true;
}

void DrmBackend::handleUdevEvent()
{
    while (auto device = m_udevMonitor->getDevice()) {
        if (!session()->isActive()) {
            continue;
        }

        if (device->action() == QStringLiteral("add")) {
            if (!m_explicitGpus.isEmpty() && !m_explicitGpus.contains(device->devNode())) {
                continue;
            }
            qCDebug(KWIN_DRM) << "New gpu found:" << device->devNode();
            if (addGpu(device->devNode())) {
                updateOutputs();
                updateCursor();
            }
        } else if (device->action() == QStringLiteral("remove")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (gpu) {
                if (primaryGpu() == gpu) {
                    qCCritical(KWIN_DRM) << "Primary gpu has been removed! Quitting...";
                    kwinApp()->quit();
                    return;
                } else {
                    qCDebug(KWIN_DRM) << "Removing gpu" << gpu->devNode();
                    emit gpuRemoved(gpu);
                    m_gpus.removeOne(gpu);
                    delete gpu;
                    updateOutputs();
                    updateCursor();
                }
            }
        } else if (device->action() == QStringLiteral("change")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (!gpu) {
                gpu = addGpu(device->devNode());
            }
            if (gpu) {
                qCDebug(KWIN_DRM) << "Received hot plug event for monitored drm device" << gpu->devNode();
                updateOutputs();
                updateCursor();
            }
        }
    }
}

DrmGpu *DrmBackend::addGpu(const QString &fileName)
{
    if (primaryGpu() && primaryGpu()->useEglStreams()) {
        return nullptr;
    }
    int fd = session()->openRestricted(fileName);
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << fileName;
        return nullptr;
    }

    // try to make a simple drm get resource call, if it fails it is not useful for us
    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        qCDebug(KWIN_DRM) << "Skipping KMS incapable drm device node at" << fileName;
        session()->closeRestricted(fd);
        return nullptr;
    }
    drmModeFreeResources(resources);

    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        qCDebug(KWIN_DRM, "Failed to fstat %s: %s", qPrintable(fileName), strerror(errno));
        session()->closeRestricted(fd);
        return nullptr;
    }

    DrmGpu *gpu = new DrmGpu(this, fileName, fd, buf.st_rdev);
    m_gpus.append(gpu);
    m_active = true;
    connect(gpu, &DrmGpu::outputAdded, this, &DrmBackend::addOutput);
    connect(gpu, &DrmGpu::outputRemoved, this, &DrmBackend::removeOutput);
    emit gpuAdded(gpu);
    return gpu;
}

void DrmBackend::addOutput(DrmOutput *o)
{
    m_outputs.append(o);
    m_enabledOutputs.append(o);
    emit o->gpu()->outputEnabled(o);
    emit outputAdded(o);
    emit outputEnabled(o);
}

void DrmBackend::removeOutput(DrmOutput *o)
{
    emit o->gpu()->outputDisabled(o);
    if (m_enabledOutputs.removeOne(o)) {
        emit outputDisabled(o);
    }
    m_outputs.removeOne(o);
    emit outputRemoved(o);
}

bool DrmBackend::updateOutputs()
{
    if (m_gpus.size() == 0) {
        return false;
    }
    const auto oldOutputs = m_outputs;
    for (auto it = m_gpus.begin(); it < m_gpus.end();) {
        auto gpu = *it;
        gpu->updateOutputs();
        if (gpu->outputs().isEmpty() && gpu != primaryGpu()) {
            qCDebug(KWIN_DRM) << "removing unused GPU" << gpu->devNode();
            it = m_gpus.erase(it);
            emit gpuRemoved(gpu);
            delete gpu;
        } else {
            it++;
        }
    }

    std::sort(m_outputs.begin(), m_outputs.end(), [] (DrmOutput *a, DrmOutput *b) { return a->m_conn->id() < b->m_conn->id(); });
    if (oldOutputs != m_outputs) {
        readOutputsConfiguration();
    }
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
    const QString uuid = generateOutputConfigurationUuid();
    const auto outputGroup = kwinApp()->config()->group("DrmOutputs");
    const auto configGroup = outputGroup.group(uuid);
    // default position goes from left to right
    QPoint pos(0, 0);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        qCDebug(KWIN_DRM) << "Reading output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        const auto outputConfig = configGroup.group((*it)->uuid().toString(QUuid::WithoutBraces));
        (*it)->setGlobalPos(outputConfig.readEntry<QPoint>("Position", pos));
        if (outputConfig.hasKey("Scale"))
            (*it)->setScale(outputConfig.readEntry("Scale", 1.0));
        (*it)->setTransformInternal(stringToTransform(outputConfig.readEntry("Transform", "normal")));
        pos.setX(pos.x() + (*it)->geometry().width());
        if (outputConfig.hasKey("Mode")) {
            QString mode = outputConfig.readEntry("Mode");
            QStringList list = mode.split("_");
            if (list.size() > 1) {
                QStringList size = list[0].split("x");
                if (size.size() > 1) {
                    int width = size[0].toInt();
                    int height = size[1].toInt();
                    int refreshRate = list[1].toInt();
                    (*it)->updateMode(width, height, refreshRate);
                }
            }
        }
    }
}

void DrmBackend::writeOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QString uuid = generateOutputConfigurationUuid();
    auto configGroup = KSharedConfig::openConfig()->group("DrmOutputs").group(uuid);
    // default position goes from left to right
    for (auto it = m_outputs.cbegin(); it != m_outputs.cend(); ++it) {
        qCDebug(KWIN_DRM) << "Writing output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        auto outputConfig = configGroup.group((*it)->uuid().toString(QUuid::WithoutBraces));
        outputConfig.writeEntry("Scale", (*it)->scale());
        outputConfig.writeEntry("Transform", transformToString((*it)->transform()));
        QString mode;
        mode += QString::number((*it)->modeSize().width());
        mode += "x";
        mode += QString::number((*it)->modeSize().height());
        mode += "_";
        mode += QString::number((*it)->refreshRate());
        outputConfig.writeEntry("Mode", mode);
    }
}

QString DrmBackend::generateOutputConfigurationUuid() const
{
    auto it = m_outputs.constBegin();
    if (m_outputs.size() == 1) {
        // special case: one output
        return (*it)->uuid().toString(QUuid::WithoutBraces);
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    for (const DrmOutput *output: qAsConst(m_outputs)) {
        hash.addData(output->uuid().toByteArray());
    }
    return QString::fromLocal8Bit(hash.result().toHex().left(10));
}

void DrmBackend::enableOutput(DrmOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_enabledOutputs.contains(output));
        m_enabledOutputs << output;
        emit output->gpu()->outputEnabled(output);
        emit outputEnabled(output);
    } else {
        Q_ASSERT(m_enabledOutputs.contains(output));
        m_enabledOutputs.removeOne(output);
        Q_ASSERT(!m_enabledOutputs.contains(output));
        emit output->gpu()->outputDisabled(output);
        emit outputDisabled(output);
    }
    checkOutputsAreOn();
    emit screensQueried();
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
    setSoftwareCursorForced(needsSoftwareCursor);
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

void DrmBackend::updateCursor()
{
    if (isSoftwareCursorForced() || isCursorHidden()) {
        return;
    }

    auto cursor = Cursors::self()->currentCursor();
    if (cursor->image().isNull()) {
        doHideCursor();
        return;
    }

    bool success = true;

    for (DrmOutput *output : qAsConst(m_outputs)) {
        success = output->updateCursor();
        if (!success) {
            qCDebug(KWIN_DRM) << "Failed to update cursor on output" << output->name();
            break;
        }
        success = output->showCursor();
        if (!success) {
            qCDebug(KWIN_DRM) << "Failed to show cursor on output" << output->name();
            break;
        }
        output->moveCursor();
    }

    setSoftwareCursor(!success);
}

void DrmBackend::doShowCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
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

void DrmBackend::doSetSoftwareCursor()
{
    if (isCursorHidden() || !usesSoftwareCursor()) {
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

QPainterBackend *DrmBackend::createQPainterBackend()
{
    return new DrmQPainterBackend(this, m_gpus.at(0));
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_EGL_STREAMS
    if (m_gpus.at(0)->useEglStreams()) {
        auto backend = new EglStreamBackend(this, m_gpus.at(0));
        AbstractEglBackend::setPrimaryBackend(backend);
        return backend;
    }
#endif

#if HAVE_GBM
    auto primaryBackend = new EglGbmBackend(this, m_gpus.at(0));
    AbstractEglBackend::setPrimaryBackend(primaryBackend);
    EglMultiBackend *backend = new EglMultiBackend(this, primaryBackend);
    for (int i = 1; i < m_gpus.count(); i++) {
        backend->addGpu(m_gpus[i]);
    }
    return backend;
#else
    return Platform::createOpenGLBackend();
#endif
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
    if (primaryGpu()->eglBackend() && primaryGpu()->gbmDevice()) {
        primaryGpu()->eglBackend()->makeCurrent();
        return GbmDmaBuf::createBuffer(size, primaryGpu()->gbmDevice());
    }
#endif
    return nullptr;
}

DrmGpu *DrmBackend::primaryGpu() const
{
    return m_gpus.isEmpty() ? nullptr : m_gpus[0];
}

DrmGpu *DrmBackend::findGpu(dev_t deviceId) const
{
    for (DrmGpu *gpu : qAsConst(m_gpus)) {
        if (gpu->deviceId() == deviceId) {
            return gpu;
        }
    }
    return nullptr;
}

}
