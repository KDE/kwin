/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_backend.h"
#include "backends/libinput/libinputbackend.h"
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
#include "scene.h"
#include "scene_qpainter_drm_backend.h"
#include "session.h"
#include "udev.h"
#include "drm_gpu.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "waylandoutputconfig.h"
#include "egl_gbm_backend.h"
#include "gbm_dmabuf.h"
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
#include <math.h>
// drm
#include <gbm.h>
#include <xf86drm.h>
#include <libdrm/drm_mode.h>

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
    setSupportsPointerWarping(true);
    setSupportsGammaControl(true);
    setPerScreenRenderingEnabled(true);
    supportsOutputChanges();
}

DrmBackend::~DrmBackend()
{
    qDeleteAll(m_gpus);
}

bool DrmBackend::isActive() const
{
    return m_active;
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

    if (Compositor::compositing()) {
        Compositor::self()->scene()->addRepaintFull();
    }

    // While the session had been inactive, an output could have been added or
    // removed, we need to re-scan outputs.
    updateOutputs();
    Q_EMIT activeChanged();
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
    Q_EMIT activeChanged();
}

bool DrmBackend::initialize()
{
    // TODO: Pause/Resume individual GPU devices instead.
    connect(session(), &Session::devicePaused, this, [this](dev_t deviceId) {
        if (primaryGpu()->deviceId() == deviceId) {
            deactivate();
        }
    });
    connect(session(), &Session::deviceResumed, this, [this](dev_t deviceId) {
        if (primaryGpu()->deviceId() == deviceId) {
            reactivate();
        }
    });
    connect(session(), &Session::awoke, this, &DrmBackend::turnOutputsOn);

    if (!m_explicitGpus.isEmpty()) {
        for (const QString &fileName : m_explicitGpus) {
            addGpu(fileName);
        }
    } else {
        const auto devices = m_udev->listGPUs();
        for (const UdevDevice::Ptr &device : devices) {
            addGpu(device->devNode());
        }
    }

    if (m_gpus.isEmpty()) {
        qCWarning(KWIN_DRM) << "No suitable DRM devices have been found";
        return false;
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
        if (!m_active) {
            continue;
        }
        if (!m_explicitGpus.isEmpty() && !m_explicitGpus.contains(device->devNode())) {
            continue;
        }

        if (device->action() == QStringLiteral("add")) {
            qCDebug(KWIN_DRM) << "New gpu found:" << device->devNode();
            if (addGpu(device->devNode())) {
                updateOutputs();
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
            }
        }
    }
}

DrmGpu *DrmBackend::addGpu(const QString &fileName)
{
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
    Q_EMIT outputAdded(o);
    enableOutput(o, true);
}

void DrmBackend::removeOutput(DrmAbstractOutput *o)
{
    enableOutput(o, false);
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
        readOutputsConfiguration(m_outputs);
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
            if (!output->isPlaceholder()) {
                hashedOutputs << outputHash(output);
            }
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

bool DrmBackend::readOutputsConfiguration(const QVector<DrmAbstractOutput*> &outputs)
{
    Q_ASSERT(!outputs.isEmpty());
    const auto outputsInfo = KWinKScreenIntegration::outputsConfig(outputs);

    AbstractWaylandOutput *primaryOutput = outputs.constFirst();
    WaylandOutputConfig cfg;
    // default position goes from left to right
    QPoint pos(0, 0);
    for (const auto &output : qAsConst(outputs)) {
        if (output->isPlaceholder()) {
            continue;
        }
        auto props = cfg.changeSet(output);
        const QJsonObject outputInfo = outputsInfo[output];
        qCDebug(KWIN_DRM) << "Reading output configuration for " << output;
        if (!outputInfo.isEmpty()) {
            if (outputInfo["primary"].toBool()) {
                primaryOutput = output;
            }
            props->enabled = outputInfo["enabled"].toBool(true);
            const QJsonObject pos = outputInfo["pos"].toObject();
            props->pos = QPoint(pos["x"].toInt(), pos["y"].toInt());
            if (const QJsonValue scale = outputInfo["scale"]; !scale.isUndefined()) {
                props->scale = scale.toDouble(1.);
            }
            props->transform = KWinKScreenIntegration::toDrmTransform(outputInfo["rotation"].toInt());

            props->overscan = static_cast<uint32_t>(outputInfo["overscan"].toInt(props->overscan));
            props->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(outputInfo["vrrpolicy"].toInt(static_cast<uint32_t>(props->vrrPolicy)));
            props->rgbRange = static_cast<AbstractWaylandOutput::RgbRange>(outputInfo["rgbrange"].toInt(static_cast<uint32_t>(props->rgbRange)));

            if (const QJsonObject mode = outputInfo["mode"].toObject(); !mode.isEmpty()) {
                const QJsonObject size = mode["size"].toObject();
                props->modeSize = QSize(size["width"].toInt(), size["height"].toInt());
                props->refreshRate = round(mode["refresh"].toDouble() * 1000);
            }
        } else {
            props->enabled = true;
            props->pos = pos;
            props->transform = DrmOutput::Transform::Normal;
        }
        pos.setX(pos.x() + output->geometry().width());
    }
    bool allDisabled = std::all_of(outputs.begin(), outputs.end(), [&cfg](const auto &output) {
        return !cfg.changeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_DRM) << "KScreen config would disable all outputs!";
        return false;
    }
    if (!cfg.changeSet(primaryOutput)->enabled) {
        qCWarning(KWIN_DRM) << "KScreen config would disable the primary output!";
        return false;
    }
    if (!applyOutputChanges(cfg)) {
        qCWarning(KWIN_DRM) << "Applying KScreen config failed!";
        return false;
    }
    setPrimaryOutput(primaryOutput);
    return true;
}

void DrmBackend::enableOutput(DrmAbstractOutput *output, bool enable)
{
    if (m_enabledOutputs.contains(output) == enable) {
        return;
    }
    if (enable) {
        m_enabledOutputs << output;
        Q_EMIT output->gpu()->outputEnabled(output);
        Q_EMIT outputEnabled(output);
        checkOutputsAreOn();
        if (m_placeHolderOutput) {
            qCDebug(KWIN_DRM) << "removing placeholder output";
            primaryGpu()->removeVirtualOutput(m_placeHolderOutput);
            m_placeHolderOutput = nullptr;
        }
    } else {
        if (m_enabledOutputs.count() == 1 && m_outputs.count() > 1) {
            auto outputs = m_outputs;
            outputs.removeOne(output);
            if (!readOutputsConfiguration(outputs)) {
                // config is invalid or failed to apply -> Try to enable an output anyways
                WaylandOutputConfig cfg;
                cfg.changeSet(outputs.constFirst())->enabled = true;
                if (!applyOutputChanges(cfg)) {
                    qCCritical(KWIN_DRM) << "Could not enable any outputs!";
                }
            }
        }
        if (m_enabledOutputs.count() == 1 && !kwinApp()->isTerminating()) {
            qCDebug(KWIN_DRM) << "adding placeholder output";
            m_placeHolderOutput = primaryGpu()->createVirtualOutput({}, m_enabledOutputs.constFirst()->pixelSize(), 1, DrmGpu::Placeholder);
            // placeholder doesn't actually need to render anything
            m_placeHolderOutput->renderLoop()->inhibit();
        }
        m_enabledOutputs.removeOne(output);
        Q_EMIT output->gpu()->outputDisabled(output);
        Q_EMIT outputDisabled(output);
    }
}

InputBackend *DrmBackend::createInputBackend()
{
    return new LibinputBackend();
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    return new DrmQPainterBackend(this);
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
    return new EglGbmBackend(this);
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
    return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
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
    return supportInfo;
}

AbstractOutput *DrmBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    auto output = primaryGpu()->createVirtualOutput(name, size * scale, scale, DrmGpu::Full);
    readOutputsConfiguration(m_outputs);
    Q_EMIT screensQueried();
    return output;
}

void DrmBackend::removeVirtualOutput(AbstractOutput *output)
{
    auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output);
    if (!virtualOutput) {
        return;
    }
    primaryGpu()->removeVirtualOutput(virtualOutput);
}

DmaBufTexture *DrmBackend::createDmaBufTexture(const QSize &size)
{
    if (primaryGpu()->eglBackend() && primaryGpu()->gbmDevice()) {
        primaryGpu()->eglBackend()->makeCurrent();
        return GbmDmaBuf::createBuffer(size, primaryGpu()->gbmDevice());
    } else {
        return nullptr;
    }
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

bool DrmBackend::applyOutputChanges(const WaylandOutputConfig &config)
{
    QVector<DrmOutput*> changed;
    for (const auto &gpu : qAsConst(m_gpus)) {
        const auto &outputs = gpu->outputs();
        for (const auto &o : outputs) {
            DrmOutput *output = qobject_cast<DrmOutput*>(o);
            if (!output) {
                // virtual outputs don't need testing
                continue;
            }
            output->queueChanges(config);
            changed << output;
        }
        if (!gpu->testPendingConfiguration()) {
            for (const auto &output : qAsConst(changed)) {
                output->revertQueuedChanges();
            }
            return false;
        }
    }
    // first, apply changes to drm outputs.
    // This may remove the placeholder output and thus change m_outputs!
    for (const auto &output : qAsConst(changed)) {
        output->applyQueuedChanges(config);
    }
    // only then apply changes to the virtual outputs
    for (const auto &output : qAsConst(m_outputs)) {
        if (!qobject_cast<DrmOutput*>(output)) {
            output->applyChanges(config);
        }
    };
    if (Compositor::compositing()) {
        Compositor::self()->scene()->addRepaintFull();
    }
    return true;
}

}
