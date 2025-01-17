/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

#include <QObject>
#include <QRect>

namespace KWin
{

class SoftwareVsyncMonitor;
class VirtualBackend;
class OutputFrame;

class VirtualOutput : public Output
{
    Q_OBJECT

public:
    explicit VirtualOutput(VirtualBackend *parent, bool internal, const QSize &physicalSizeInMM, OutputTransform panelOrientation, const QByteArray &edid, std::optional<QByteArray> edidIdentifierOverride, const std::optional<QString> &connectorName, const std::optional<QByteArray> &mstPath);
    ~VirtualOutput() override;

    RenderLoop *renderLoop() const override;
    void present(const std::shared_ptr<OutputFrame> &frame);

    void init(const QPoint &logicalPosition, const QSize &pixelSize, qreal scale, const QList<std::tuple<QSize, uint64_t, OutputMode::Flags>> &modes);
    void updateEnabled(bool enabled);

    void applyChanges(const OutputConfiguration &config) override;

private:
    void vblank(std::chrono::nanoseconds timestamp);

    Q_DISABLE_COPY(VirtualOutput);
    friend class VirtualBackend;

    VirtualBackend *m_backend;
    std::unique_ptr<RenderLoop> m_renderLoop;
    std::unique_ptr<SoftwareVsyncMonitor> m_vsyncMonitor;
    int m_gammaSize = 200;
    bool m_gammaResult = true;
    int m_identifier;
    std::shared_ptr<OutputFrame> m_frame;
};

} // namespace KWin
