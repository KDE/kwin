/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputbackend.h"

#include <QList>
#include <QPointer>
#include <QSize>
#include <QSocketNotifier>

#include <memory>
#include <sys/types.h>

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

    std::unique_ptr<InputBackend> createInputBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;
    EglDisplay *sceneEglDisplayObject() const override;

    bool initialize() override;

    Outputs outputs() const override;
    Session *session() const override;

    QList<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;
    Output *createVirtualOutput(const QString &name, const QSize &size, double scale) override;
    void removeVirtualOutput(Output *output) override;

    DrmGpu *primaryGpu() const;
    DrmGpu *findGpu(dev_t deviceId) const;
    size_t gpuCount() const;

    void setRenderBackend(DrmRenderBackend *backend);
    DrmRenderBackend *renderBackend() const;

    void releaseBuffers();
    void updateOutputs();

    const std::vector<std::unique_ptr<DrmGpu>> &gpus() const;

public Q_SLOTS:
    void sceneInitialized() override;

Q_SIGNALS:
    void gpuAdded(DrmGpu *gpu);
    void gpuRemoved(DrmGpu *gpu);

protected:
    bool applyOutputChanges(const OutputConfiguration &config) override;

private:
    friend class DrmGpu;
    void addOutput(DrmAbstractOutput *output);
    void removeOutput(DrmAbstractOutput *output);
    void handleUdevEvent();
    DrmGpu *addGpu(const QString &fileName);

    std::unique_ptr<Udev> m_udev;
    std::unique_ptr<UdevMonitor> m_udevMonitor;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    Session *m_session;
    QList<DrmAbstractOutput *> m_outputs;
    QList<QUuid> m_recentlyUnpluggedDpmsOffOutputs;

    const QStringList m_explicitGpus;
    std::vector<std::unique_ptr<DrmGpu>> m_gpus;
    DrmRenderBackend *m_renderBackend = nullptr;
};

}
