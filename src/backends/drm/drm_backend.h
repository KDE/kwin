/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_BACKEND_H
#define KWIN_DRM_BACKEND_H
#include "platform.h"

#include "dpmsinputeventfilter.h"
#include "placeholderinputeventfilter.h"

#include <QPointer>
#include <QSize>
#include <QVector>

#include <memory>

namespace KWin
{

class Udev;
class UdevMonitor;
class UdevDevice;

class DrmAbstractOutput;
class Cursor;
class DrmGpu;
class DrmVirtualOutput;

class KWIN_EXPORT DrmBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "drm.json")
public:
    explicit DrmBackend(QObject *parent = nullptr);
    ~DrmBackend() override;

    InputBackend *createInputBackend() override;
    QPainterBackend *createQPainterBackend() override;
    OpenGLBackend* createOpenGLBackend() override;
    DmaBufTexture *createDmaBufTexture(const QSize &size) override;
    Session *session() const override;
    bool initialize() override;

    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

    void enableOutput(DrmAbstractOutput *output, bool enable);

    void createDpmsFilter();
    void checkOutputsAreOn();

    QVector<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;
    AbstractOutput *createVirtualOutput(const QString &name, const QSize &size, double scale) override;
    void removeVirtualOutput(AbstractOutput *output) override;

    DrmGpu *primaryGpu() const;
    DrmGpu *findGpu(dev_t deviceId) const;
    DrmGpu *findGpuByFd(int fd) const;

    bool isActive() const;

public Q_SLOTS:
    void turnOutputsOn();
    void sceneInitialized() override;

Q_SIGNALS:
    void activeChanged();
    void gpuRemoved(DrmGpu *gpu);
    void gpuAdded(DrmGpu *gpu);

protected:
    bool applyOutputChanges(const WaylandOutputConfig &config) override;

private:
    friend class DrmGpu;
    void addOutput(DrmAbstractOutput* output);
    void removeOutput(DrmAbstractOutput* output);
    void activate(bool active);
    void reactivate();
    void deactivate();
    void updateOutputs();
    bool readOutputsConfiguration(const QVector<DrmAbstractOutput*> &outputs);
    void handleUdevEvent();
    DrmGpu *addGpu(const QString &fileName);

    QScopedPointer<Udev> m_udev;
    QScopedPointer<UdevMonitor> m_udevMonitor;
    Session *m_session = nullptr;
    // all outputs, enabled and disabled
    QVector<DrmAbstractOutput*> m_outputs;
    // only enabled outputs
    QVector<DrmAbstractOutput*> m_enabledOutputs;
    DrmVirtualOutput *m_placeHolderOutput = nullptr;

    bool m_active = false;
    const QStringList m_explicitGpus;
    QVector<DrmGpu*> m_gpus;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
    QScopedPointer<PlaceholderInputEventFilter> m_placeholderFilter;
};


}

#endif

