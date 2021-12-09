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
#include "drm_gpu.h"
#include "egl_multi_backend.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
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
#include <KCoreAddons>
#include <KLocalizedString>
// Qt
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include "drm_pipeline.h"

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
    m_dpmsFilter.reset(new DpmsInputEventFilter);
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
        if ((*it)->dpmsMode() != AbstractWaylandOutput::DpmsMode::On) {
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

    for (const auto &output : qAsConst(m_outputs)) {
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

    for (const auto &output : qAsConst(m_outputs)) {
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
    // workaround for BUG 438363: something goes wrong in scene initialization without a surface being current in EglStreamBackend
    if (m_gpus[0]->useEglStreams()) {
        updateOutputs();
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
        if (!m_explicitGpus.isEmpty() && !m_explicitGpus.contains(device->devNode())) {
            continue;
        }

        if (device->action() == QStringLiteral("add")) {
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
                    Q_EMIT gpuRemoved(gpu);
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
                qCDebug(KWIN_DRM) << "Received change event for monitored drm device" << gpu->devNode();
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
    Q_EMIT gpuAdded(gpu);
    return gpu;
}

void DrmBackend::addOutput(DrmAbstractOutput *o)
{
    m_outputs.append(o);
    m_enabledOutputs.append(o);
    Q_EMIT outputAdded(o);
    Q_EMIT outputEnabled(o);
    if (m_placeHolderOutput) {
        qCDebug(KWIN_DRM) << "removing placeholder output";
        primaryGpu()->removeVirtualOutput(m_placeHolderOutput);
        m_placeHolderOutput = nullptr;
    }
    checkOutputsAreOn();
}

void DrmBackend::removeOutput(DrmAbstractOutput *o)
{
    if (m_outputs.count() == 1 && !kwinApp()->isTerminating()) {
        qCDebug(KWIN_DRM) << "adding placeholder output";
        m_placeHolderOutput = primaryGpu()->createVirtualOutput();
        // placeholder doesn't actually need to render anything
        m_placeHolderOutput->renderLoop()->inhibit();
    }
    if (m_enabledOutputs.contains(o)) {
        if (m_enabledOutputs.count() == 1) {
            for (const auto &output : qAsConst(m_outputs)) {
                if (output != o) {
                    output->setEnabled(true);
                    break;
                }
            }
        }
        m_enabledOutputs.removeOne(o);
        Q_EMIT outputDisabled(o);
    }
    m_outputs.removeOne(o);
    Q_EMIT outputRemoved(o);
}

void DrmBackend::updateOutputs()
{
    const auto oldOutputs = m_outputs;
    for (auto it = m_gpus.begin(); it < m_gpus.end();) {
        auto gpu = *it;
        gpu->updateOutputs();
        if (gpu->outputs().isEmpty() && gpu != primaryGpu()) {
            qCDebug(KWIN_DRM) << "removing unused GPU" << gpu->devNode();
            it = m_gpus.erase(it);
            Q_EMIT gpuRemoved(gpu);
            delete gpu;
        } else {
            it++;
        }
    }

    std::sort(m_outputs.begin(), m_outputs.end(), [] (DrmAbstractOutput *a, DrmAbstractOutput *b) {
        auto da = qobject_cast<DrmOutput *>(a);
        auto db = qobject_cast<DrmOutput *>(b);
        if (da && !db) {
            return true;
        } else if (da && db) {
            return da->pipeline()->connector()->id() < db->pipeline()->connector()->id();
        } else {
            return false;
        }
    });
    if (oldOutputs != m_outputs) {
        readOutputsConfiguration();
    }
    Q_EMIT screensQueried();
}

namespace KWinKScreenIntegration
{
    /// See KScreen::Output::hashMd5
    QString outputHash(DrmAbstractOutput *output)
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (!output->edid().isEmpty()) {
            hash.addData(output->edid());
        } else {
            hash.addData(output->name().toLatin1());
        }
        return QString::fromLatin1(hash.result().toHex());
    }

    /// See KScreen::Config::connectedOutputsHash in libkscreen
    QString connectedOutputsHash(const QVector<DrmAbstractOutput*> &outputs)
    {
        QStringList hashedOutputs;
        hashedOutputs.reserve(outputs.count());
        for (auto output : qAsConst(outputs)) {
            hashedOutputs << outputHash(output);
        }
        std::sort(hashedOutputs.begin(), hashedOutputs.end());
        const auto hash = QCryptographicHash::hash(hashedOutputs.join(QString()).toLatin1(), QCryptographicHash::Md5);
        return QString::fromLatin1(hash.toHex());
    }

    QMap<DrmAbstractOutput*, QJsonObject> outputsConfig(const QVector<DrmAbstractOutput*> &outputs)
    {
        const QString kscreenJsonPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kscreen/") % connectedOutputsHash(outputs));
        if (kscreenJsonPath.isEmpty()) {
            return {};
        }

        QFile f(kscreenJsonPath);
        if (!f.open(QIODevice::ReadOnly)) {
            qCWarning(KWIN_DRM) << "Could not open file" << kscreenJsonPath;
            return {};
        }

        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            qCWarning(KWIN_DRM) << "Failed to parse" << kscreenJsonPath << error.errorString();
            return {};
        }

        QMap<DrmAbstractOutput*, QJsonObject> ret;
        const auto outputsJson = doc.array();
        for (const auto &outputJson : outputsJson) {
            const auto outputObject = outputJson.toObject();
            for (auto it = outputs.constBegin(), itEnd = outputs.constEnd(); it != itEnd; ) {
                if (!ret.contains(*it) && outputObject["id"] == outputHash(*it)) {
                    ret[*it] = outputObject;
                    continue;
                }
                ++it;
            }
        }
        return ret;
    }

    /// See KScreen::Output::Rotation
    enum Rotation {
        None = 1,
        Left = 2,
        Inverted = 4,
        Right = 8,
    };

    DrmOutput::Transform toDrmTransform(int rotation)
    {
        switch (Rotation(rotation)) {
        case None:
            return DrmOutput::Transform::Normal;
        case Left:
            return DrmOutput::Transform::Rotated90;
        case Inverted:
            return DrmOutput::Transform::Rotated180;
        case Right:
            return DrmOutput::Transform::Rotated270;
        default:
            Q_UNREACHABLE();
        }
    }
}

void DrmBackend::readOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const auto outputsInfo = KWinKScreenIntegration::outputsConfig(m_outputs);

    // default position goes from left to right
    QPoint pos(0, 0);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        const QJsonObject outputInfo = outputsInfo[*it];
        qCDebug(KWIN_DRM) << "Reading output configuration for " << *it;
        if (!outputInfo.isEmpty()) {
            const QJsonObject pos = outputInfo["pos"].toObject();
            (*it)->moveTo({pos["x"].toInt(), pos["y"].toInt()});
            if (const QJsonValue scale = outputInfo["scale"]; !scale.isUndefined()) {
                (*it)->setScale(scale.toDouble(1.));
            }
            (*it)->updateTransform(KWinKScreenIntegration::toDrmTransform(outputInfo["rotation"].toInt()));

            if (const QJsonObject mode = outputInfo["mode"].toObject(); !mode.isEmpty()) {
                const QJsonObject size = mode["size"].toObject();
                (*it)->updateMode(QSize(size["width"].toInt(), size["height"].toInt()), mode["refresh"].toDouble() * 1000);
            }
        } else {
            (*it)->moveTo(pos);
            (*it)->updateTransform(DrmOutput::Transform::Normal);
        }
        pos.setX(pos.x() + (*it)->geometry().width());
    }
}

void DrmBackend::enableOutput(DrmAbstractOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_enabledOutputs.contains(output));
        m_enabledOutputs << output;
        Q_EMIT output->gpu()->outputEnabled(output);
        Q_EMIT outputEnabled(output);
    } else {
        Q_ASSERT(m_enabledOutputs.contains(output));
        m_enabledOutputs.removeOne(output);
        Q_ASSERT(!m_enabledOutputs.contains(output));
        Q_EMIT output->gpu()->outputDisabled(output);
        Q_EMIT outputDisabled(output);
    }
    checkOutputsAreOn();
    Q_EMIT screensQueried();
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

    for (DrmAbstractOutput *output : qAsConst(m_outputs)) {
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
        success = output->moveCursor();
        if (!success) {
            qCDebug(KWIN_DRM) << "Failed to move cursor on output" << output->name();
            break;
        }
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

void DrmBackend::sceneInitialized()
{
    updateOutputs();
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

DrmGpu *DrmBackend::findGpuByFd(int fd) const
{
    for (DrmGpu *gpu : qAsConst(m_gpus)) {
        if (gpu->fd() == fd) {
            return gpu;
        }
    }
    return nullptr;
}

}
