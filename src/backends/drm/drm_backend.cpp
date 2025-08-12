/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_backend.h"

#include "config-kwin.h"

#include "backends/libinput/libinputbackend.h"
#include "core/outputconfiguration.h"
#include "core/session.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_qpainter_backend.h"
#include "drm_render_backend.h"
#include "drm_virtual_output.h"
#include "utils/envvar.h"
#include "utils/udev.h"
// KF5
#include <KCoreAddons>
#include <KLocalizedString>
// Qt
#include <QCoreApplication>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QStringBuilder>
// system
#include <algorithm>
#include <cerrno>
#include <ranges>
#include <sys/stat.h>
#include <unistd.h>
// drm
#include <gbm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>

namespace KWin
{

static QStringList splitPathList(const QString &input, const QChar delimiter)
{
    QStringList ret;
    QString tmp;
    for (int i = 0; i < input.size(); i++) {
        if (input[i] == delimiter) {
            if (i > 0 && input[i - 1] == '\\') {
                tmp[tmp.size() - 1] = delimiter;
            } else if (!tmp.isEmpty()) {
                ret.append(tmp);
                tmp = QString();
            }
        } else {
            tmp.append(input[i]);
        }
    }
    if (!tmp.isEmpty()) {
        ret.append(tmp);
    }
    return ret;
}

DrmBackend::DrmBackend(Session *session, QObject *parent)
    : OutputBackend(parent)
    , m_udev(std::make_unique<Udev>())
    , m_udevMonitor(m_udev->monitor())
    , m_session(session)
    , m_explicitGpus(splitPathList(qEnvironmentVariable("KWIN_DRM_DEVICES"), ':'))
{
}

DrmBackend::~DrmBackend() = default;

Session *DrmBackend::session() const
{
    return m_session;
}

Outputs DrmBackend::outputs() const
{
    return m_outputs;
}

bool DrmBackend::initialize()
{
    connect(m_session, &Session::devicePaused, this, [this](dev_t deviceId) {
        if (const auto gpu = findGpu(deviceId)) {
            gpu->setActive(false);
        }
    });
    connect(m_session, &Session::deviceResumed, this, [this](dev_t deviceId) {
        if (const auto gpu = findGpu(deviceId); gpu && !gpu->isActive()) {
            gpu->setActive(true);
            // the output list might've changed while the device was inactive
            // note that this might delete the gpu!
            updateOutputs();
        }
    });

    if (!m_explicitGpus.isEmpty()) {
        for (const QString &fileName : m_explicitGpus) {
            addGpu(fileName);
        }
    } else {
        const auto devices = m_udev->listGPUs();
        for (const auto &device : devices) {
            if (device->seat() == m_session->seat()) {
                addGpu(device->devNode());
            }
        }
    }

    if (m_gpus.empty()) {
        qCWarning(KWIN_DRM) << "No suitable DRM devices have been found";
        return false;
    }

    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            m_socketNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
            connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &DrmBackend::handleUdevEvent);
            m_udevMonitor->enable();
        }
    }
    updateOutputs();

    if (m_explicitGpus.empty() && m_gpus.size() > 1) {
        std::ranges::sort(m_gpus, [](const auto &gpu1, const auto &gpu2) {
            const size_t internalOutputs1 = std::ranges::count_if(gpu1->drmOutputs(), &LogicalOutput::isInternal);
            const size_t internalOutputs2 = std::ranges::count_if(gpu2->drmOutputs(), &LogicalOutput::isInternal);
            if (internalOutputs1 != internalOutputs2) {
                return internalOutputs1 > internalOutputs2;
            }
            const size_t desktopOutputs1 = std::ranges::count_if(gpu1->drmOutputs(), std::not_fn(&LogicalOutput::isNonDesktop));
            const size_t desktopOutputs2 = std::ranges::count_if(gpu2->drmOutputs(), std::not_fn(&LogicalOutput::isNonDesktop));
            if (desktopOutputs1 != desktopOutputs2) {
                return desktopOutputs1 > desktopOutputs2;
            }
            return gpu1->drmOutputs().size() > gpu2->drmOutputs().size();
        });
        qCDebug(KWIN_DRM) << "chose" << m_gpus.front()->drmDevice()->path() << "as the primary GPU";
    }
    return true;
}

void DrmBackend::handleUdevEvent()
{
    while (auto device = m_udevMonitor->getDevice()) {
        // Ignore the device seat if the KWIN_DRM_DEVICES envvar is set.
        if (!m_explicitGpus.isEmpty()) {
            const auto canonicalPath = QFileInfo(device->devNode()).canonicalFilePath();
            const bool foundMatch = std::ranges::any_of(m_explicitGpus, [&canonicalPath](const QString &explicitPath) {
                return QFileInfo(explicitPath).canonicalFilePath() == canonicalPath;
            });
            if (!foundMatch) {
                continue;
            }
        } else {
            if (device->seat() != m_session->seat()) {
                continue;
            }
        }

        if (device->action() == QLatin1StringView("add")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (gpu) {
                qCWarning(KWIN_DRM) << "Received unexpected add udev event for:" << device->devNode();
                continue;
            }
            if (addGpu(device->devNode())) {
                updateOutputs();
            }
        } else if (device->action() == QLatin1StringView("remove")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (gpu) {
                if (primaryGpu() == gpu) {
                    qCCritical(KWIN_DRM) << "Primary gpu has been removed! Quitting...";
                    QCoreApplication::exit(1);
                    return;
                } else {
                    gpu->setRemoved();
                    updateOutputs();
                }
            }
        } else if (device->action() == QLatin1StringView("change")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (!gpu) {
                gpu = addGpu(device->devNode());
            }
            if (gpu && gpu->isActive()) {
                qCDebug(KWIN_DRM) << "Received change event for monitored drm device" << gpu->drmDevice()->path();
                updateOutputs();
            }
        }
    }
}

DrmGpu *DrmBackend::addGpu(const QString &fileName)
{
    int fd = m_session->openRestricted(fileName);
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << fileName;
        return nullptr;
    }

    if (!drmIsKMS(fd)) {
        qCDebug(KWIN_DRM) << "Skipping KMS incapable drm device node at" << fileName;
        m_session->closeRestricted(fd);
        return nullptr;
    }

    auto drmDevice = DrmDevice::openWithAuthentication(fileName, fd);
    if (!drmDevice) {
        m_session->closeRestricted(fd);
        return nullptr;
    }

    m_gpus.push_back(std::make_unique<DrmGpu>(this, fd, std::move(drmDevice)));
    auto gpu = m_gpus.back().get();
    qCDebug(KWIN_DRM, "adding GPU %s", qPrintable(fileName));
    connect(gpu, &DrmGpu::outputAdded, this, &DrmBackend::addOutput);
    connect(gpu, &DrmGpu::outputRemoved, this, &DrmBackend::removeOutput);
    Q_EMIT gpuAdded(gpu);
    return gpu;
}

static QString earlyIdentifier(LogicalOutput *output)
{
    // We can't use the output's UUID because that's only set later, by the output config system.
    // This doesn't need to be perfectly accurate though, sometimes getting a false positive is ok,
    // so this just uses EDID ID, EDID hash or connector name, whichever is available
    if (output->edid().isValid()) {
        if (!output->edid().identifier().isEmpty()) {
            return output->edid().identifier();
        } else {
            return output->edid().hash();
        }
    } else {
        return output->name();
    }
}

void DrmBackend::addOutput(DrmAbstractOutput *o)
{
    const bool allOff = std::ranges::all_of(m_outputs, [](LogicalOutput *output) {
        return !output->isEnabled() || output->dpmsMode() != LogicalOutput::DpmsMode::On;
    });
    if (allOff && m_recentlyUnpluggedDpmsOffOutputs.contains(earlyIdentifier(o))) {
        if (DrmOutput *drmOutput = qobject_cast<DrmOutput *>(o)) {
            // When the system is in dpms power saving mode, KWin turns on all outputs if the user plugs a new output in
            // as that's an intentional action and they expect to see the output light up.
            // Some outputs however temporarily disconnect in some situations, most often shortly after they go into standby.
            // To not turn on outputs in that case, restore the previous dpms state
            drmOutput->updateDpmsMode(LogicalOutput::DpmsMode::Off);
            drmOutput->pipeline()->setActive(false);
            drmOutput->renderLoop()->inhibit();
            m_recentlyUnpluggedDpmsOffOutputs.removeOne(earlyIdentifier(drmOutput));
        }
    }
    m_outputs.append(o);
    Q_EMIT outputAdded(o);
}

static const int s_dpmsTimeout = environmentVariableIntValue("KWIN_DPMS_WORKAROUND_TIMEOUT").value_or(2000);

void DrmBackend::removeOutput(DrmAbstractOutput *o)
{
    if (o->dpmsMode() == LogicalOutput::DpmsMode::Off) {
        const QString id = earlyIdentifier(o);
        m_recentlyUnpluggedDpmsOffOutputs.push_back(id);
        QTimer::singleShot(s_dpmsTimeout, this, [this, id]() {
            m_recentlyUnpluggedDpmsOffOutputs.removeOne(id);
        });
    }
    m_outputs.removeOne(o);
    Q_EMIT outputRemoved(o);
}

void DrmBackend::updateOutputs()
{
    for (auto it = m_gpus.begin(); it != m_gpus.end(); ++it) {
        if ((*it)->isRemoved()) {
            (*it)->removeOutputs();
        } else {
            (*it)->updateOutputs();
        }
    }

    Q_EMIT outputsQueried();

    for (auto it = m_gpus.begin(); it != m_gpus.end();) {
        DrmGpu *gpu = it->get();
        if (gpu->isRemoved() || (gpu != primaryGpu() && gpu->drmOutputs().isEmpty())) {
            qCDebug(KWIN_DRM) << "Removing GPU" << it->get();
            const std::unique_ptr<DrmGpu> keepAlive = std::move(*it);
            it = m_gpus.erase(it);
            Q_EMIT gpuRemoved(keepAlive.get());
        } else {
            it++;
        }
    }
}

std::unique_ptr<InputBackend> DrmBackend::createInputBackend()
{
    return std::make_unique<LibinputBackend>(m_session);
}

std::unique_ptr<QPainterBackend> DrmBackend::createQPainterBackend()
{
    return std::make_unique<DrmQPainterBackend>(this);
}

std::unique_ptr<EglBackend> DrmBackend::createOpenGLBackend()
{
    return std::make_unique<EglGbmBackend>(this);
}

QList<CompositingType> DrmBackend::supportedCompositors() const
{
    return QList<CompositingType>{OpenGLCompositing, QPainterCompositing};
}

QString DrmBackend::supportInformation() const
{
    QString supportInfo;
    QDebug s(&supportInfo);
    s.nospace();
    s << "Name: "
      << "DRM" << Qt::endl;
    for (size_t g = 0; g < m_gpus.size(); g++) {
        s << "Atomic Mode Setting on GPU " << g << ": " << m_gpus.at(g)->atomicModeSetting() << Qt::endl;
    }
    return supportInfo;
}

LogicalOutput *DrmBackend::createVirtualOutput(const QString &name, const QString &description, const QSize &size, double scale)
{
    const auto ret = new DrmVirtualOutput(this, name, description, size, scale);
    m_virtualOutputs.push_back(ret);
    addOutput(ret);
    Q_EMIT outputsQueried();
    return ret;
}

void DrmBackend::removeVirtualOutput(LogicalOutput *output)
{
    auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output);
    Q_ASSERT(virtualOutput);
    if (!m_virtualOutputs.removeOne(virtualOutput)) {
        return;
    }
    removeOutput(virtualOutput);
    Q_EMIT outputsQueried();
    virtualOutput->unref();
}

DrmGpu *DrmBackend::primaryGpu() const
{
    return m_gpus.empty() ? nullptr : m_gpus.front().get();
}

DrmGpu *DrmBackend::findGpu(dev_t deviceId) const
{
    auto it = std::ranges::find_if(m_gpus, [deviceId](const auto &gpu) {
        return gpu->drmDevice()->deviceId() == deviceId;
    });
    return it == m_gpus.end() ? nullptr : it->get();
}

size_t DrmBackend::gpuCount() const
{
    return m_gpus.size();
}

OutputConfigurationError DrmBackend::applyOutputChanges(const OutputConfiguration &config)
{
    QList<DrmOutput *> toBeEnabled;
    QList<DrmOutput *> toBeDisabled;
    for (const auto &gpu : m_gpus) {
        const auto outputs = gpu->drmOutputs();
        for (DrmOutput *output : outputs) {
            if (output->isNonDesktop()) {
                continue;
            }
            if (const auto changeset = config.constChangeSet(output)) {
                output->queueChanges(changeset);
                if (changeset->enabled.value_or(output->isEnabled())) {
                    toBeEnabled << output;
                } else {
                    toBeDisabled << output;
                }
            }
        }
        const auto error = gpu->testPendingConfiguration();
        if (error != DrmPipeline::Error::None) {
            for (DrmOutput *output : std::as_const(toBeEnabled)) {
                output->revertQueuedChanges();
            }
            for (DrmOutput *output : std::as_const(toBeDisabled)) {
                output->revertQueuedChanges();
            }
            if (error == DrmPipeline::Error::NotEnoughCrtcs) {
                // TODO make this more specific, this is per GPU!
                return OutputConfigurationError::TooManyEnabledOutputs;
            } else {
                return OutputConfigurationError::Unknown;
            }
        }
    }
    // first, apply changes to drm outputs.
    // This may remove the placeholder output and thus change m_outputs!
    for (DrmOutput *output : std::as_const(toBeEnabled)) {
        if (const auto changeset = config.constChangeSet(output)) {
            output->applyQueuedChanges(changeset);
        }
    }
    for (DrmOutput *output : std::as_const(toBeDisabled)) {
        if (const auto changeset = config.constChangeSet(output)) {
            output->applyQueuedChanges(changeset);
        }
    }
    // only then apply changes to the virtual outputs
    for (DrmVirtualOutput *output : std::as_const(m_virtualOutputs)) {
        output->applyChanges(config);
    }
    return OutputConfigurationError::None;
}

void DrmBackend::setRenderBackend(DrmRenderBackend *backend)
{
    m_renderBackend = backend;
}

DrmRenderBackend *DrmBackend::renderBackend() const
{
    return m_renderBackend;
}

void DrmBackend::createLayers()
{
    for (const auto &gpu : m_gpus) {
        gpu->recreateSurfaces();
    }
    for (DrmVirtualOutput *virt : std::as_const(m_virtualOutputs)) {
        virt->recreateSurface();
    }
}

void DrmBackend::releaseBuffers()
{
    for (const auto &gpu : m_gpus) {
        gpu->releaseBuffers();
    }
    for (const DrmVirtualOutput *virt : std::as_const(m_virtualOutputs)) {
        virt->primaryLayer()->releaseBuffers();
    }
}

const std::vector<std::unique_ptr<DrmGpu>> &DrmBackend::gpus() const
{
    return m_gpus;
}

EglDisplay *DrmBackend::sceneEglDisplayObject() const
{
    return m_gpus.front()->eglDisplay();
}
}

#include "moc_drm_backend.cpp"
