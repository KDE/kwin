/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_OUTPUT_H
#define KWIN_VIRTUAL_OUTPUT_H

#include "abstract_wayland_output.h"

#include <QObject>
#include <QRect>

namespace KWin
{

class SoftwareVsyncMonitor;
class VirtualBackend;

class VirtualOutput : public AbstractWaylandOutput
{
    Q_OBJECT

public:
    VirtualOutput(VirtualBackend *parent = nullptr);
    ~VirtualOutput() override;

    RenderLoop *renderLoop() const override;
    SoftwareVsyncMonitor *vsyncMonitor() const;

    void init(const QPoint &logicalPosition, const QSize &pixelSize);

    void setGeometry(const QRect &geo);

    int gammaRampSize() const override {
        return m_gammaSize;
    }
    bool setGammaRamp(const GammaRamp &gamma) override {
        Q_UNUSED(gamma);
        return m_gammaResult;
    }

    void updateEnablement(bool enable) override;

private:
    void vblank(std::chrono::nanoseconds timestamp);

    Q_DISABLE_COPY(VirtualOutput);
    friend class VirtualBackend;

    VirtualBackend *m_backend;
    RenderLoop *m_renderLoop;
    SoftwareVsyncMonitor *m_vsyncMonitor;
    int m_gammaSize = 200;
    bool m_gammaResult = true;
    int m_identifier;
};

}

#endif
