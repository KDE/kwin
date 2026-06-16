/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "core/outputbackend.h"
#include "utils/filedescriptor.h"

namespace KWin
{

class VirtualBackend;
class VirtualOutput;
class DrmDevice;
class RenderDevice;

class KWIN_EXPORT VirtualBackend : public OutputBackend
{
    Q_OBJECT

public:
    VirtualBackend(QObject *parent = nullptr);
    ~VirtualBackend() override;

    bool initialize() override;

    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<EglBackend> createOpenGLBackend() override;

    BackendOutput *createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale) override;
    void removeVirtualOutput(BackendOutput *output) override;

    struct OutputInfo
    {
        QSize size;
        double scale = 1;
        bool internal = false;
        QSize physicalSizeInMM;
        QList<OutputModeline> modes;
        OutputTransform panelOrientation = OutputTransform::Kind::Normal;
        QByteArray edid;
        std::optional<QByteArray> edidIdentifierOverride;
        std::optional<QString> connectorName;
        std::optional<QByteArray> mstPath;
    };
    BackendOutput *addOutput(const OutputInfo &info);
    void setVirtualOutputs(const QList<OutputInfo> &infos);

    QList<BackendOutput *> outputs() const override;

    QList<CompositingType> supportedCompositors() const override;

    RenderDevice *renderDevice() const;

    DrmDevice *drmDevice() const;

    /**
     * The function passed in will be called before output changes are applied,
     * and any result other than "None" will prevent the config from being applied.
     * This should only be used by autotests.
     */
    void setOutputChangeCheck(const std::function<OutputConfigurationError(const OutputConfiguration &config)> &check);
    OutputConfigurationError applyOutputChanges(const OutputConfiguration &config) override;

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    VirtualOutput *createOutput(const OutputInfo &info);
    void removeOutput(VirtualOutput *output);

    QList<VirtualOutput *> m_outputs;
    RenderDevice *m_renderDevice = nullptr;
    std::function<OutputConfigurationError(const OutputConfiguration &config)> m_outputChangeCheck;
};

} // namespace KWin
