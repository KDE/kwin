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

class VirtualOutput : public Output
{
    Q_OBJECT

public:
    VirtualOutput(VirtualBackend *parent = nullptr);
    ~VirtualOutput() override;

    RenderLoop *renderLoop() const override;
    SoftwareVsyncMonitor *vsyncMonitor() const;

    void init(const QPoint &logicalPosition, const QSize &pixelSize, qreal scale);
    void updateEnabled(bool enabled);

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
};

} // namespace KWin
