/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputbackend.h"

#include "dpmsinputeventfilter.h"

#include <QPointer>
#include <QSize>
#include <QSocketNotifier>
#include <QVector>

#include <memory>

struct gbm_bo;

namespace KWin
{

class Session;
class Udev;
class UdevMonitor;
class UdevDevice;

class DrmAbstractOutput;
class Cursor;
class DrmGpu;
class DrmVirtualOutput;
class DrmRenderBackend;

class KWIN_EXPORT DrmBackend : public OutputBackend
{
    Q_OBJECT

public:
    explicit DrmBackend(Session *session, QObject *parent = nullptr);
    ~DrmBackend() override;

    Session *session() const;

    std::unique_ptr<InputBackend> createInputBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;

    std::optional<DmaBufParams> testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers) override;
    std::shared_ptr<DmaBufTexture> createDmaBufTexture(const QSize &size, quint32 format, const uint64_t modifier) override;
    bool initialize() override;

    Outputs outputs() const override;

    void createDpmsFilter();
    void checkOutputsAreOn();

    QVector<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;
    Output *createVirtualOutput(const QString &name, const QSize &size, double scale) override;
    void removeVirtualOutput(Output *output) override;

    DrmGpu *primaryGpu() const;
    DrmGpu *findGpu(dev_t deviceId) const;
    size_t gpuCount() const;

    bool isActive() const;

    void setRenderBackend(DrmRenderBackend *backend);
    DrmRenderBackend *renderBackend() const;

    void releaseBuffers();
    void updateOutputs();

    const std::vector<std::unique_ptr<DrmGpu>> &gpus() const;

public Q_SLOTS:
    void turnOutputsOn();
    void sceneInitialized() override;

Q_SIGNALS:
    void activeChanged();
    void gpuAdded(DrmGpu *gpu);
    void gpuRemoved(DrmGpu *gpu);

protected:
    bool applyOutputChanges(const OutputConfiguration &config) override;

private:
    friend class DrmGpu;
    void addOutput(DrmAbstractOutput *output);
    void removeOutput(DrmAbstractOutput *output);
    void activate(bool active);
    void reactivate();
    void deactivate();
    void handleUdevEvent();
    DrmGpu *addGpu(const QString &fileName);

    std::unique_ptr<Udev> m_udev;
    std::unique_ptr<UdevMonitor> m_udevMonitor;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    Session *m_session;
    QVector<DrmAbstractOutput *> m_outputs;
    DrmVirtualOutput *m_placeHolderOutput = nullptr;

    bool m_active = false;
    const QStringList m_explicitGpus;
    std::vector<std::unique_ptr<DrmGpu>> m_gpus;
    std::unique_ptr<DpmsInputEventFilter> m_dpmsFilter;
    DrmRenderBackend *m_renderBackend = nullptr;

    gbm_bo *createBo(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers);
};

}
