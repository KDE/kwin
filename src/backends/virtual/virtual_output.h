/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/backendoutput.h"

#include <QObject>

namespace KWin
{

class SoftwareVsyncMonitor;
class VirtualBackend;
class OutputFrame;

class VirtualOutput : public BackendOutput
{
    Q_OBJECT

public:
    explicit VirtualOutput(VirtualBackend *parent, bool internal, const QSize &physicalSizeInMM, OutputTransform panelOrientation, const QByteArray &edid, std::optional<QByteArray> edidIdentifierOverride, const std::optional<QString> &connectorName, const std::optional<QByteArray> &mstPath);
    ~VirtualOutput() override;

    std::expected<void, OutputError> testPresentation(const std::shared_ptr<OutputFrame> &frame) override;
    std::expected<void, OutputError> present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame) override;

    void init(const QSize &pixelSize, qreal scale, const QList<OutputModeline> &modes);

    void applyChanges(const OutputConfiguration &config) override;

    void setOutputLayer(std::unique_ptr<OutputLayer> &&layer);
    OutputLayer *outputLayer() const;

private:
    void vblank(std::chrono::nanoseconds timestamp);

    friend class VirtualBackend;

    std::unique_ptr<OutputLayer> m_layer;
    VirtualBackend *m_backend;
    std::unique_ptr<SoftwareVsyncMonitor> m_vsyncMonitor;
    int m_gammaSize = 200;
    bool m_gammaResult = true;
    int m_identifier;
    std::shared_ptr<OutputFrame> m_frame;
};

} // namespace KWin
