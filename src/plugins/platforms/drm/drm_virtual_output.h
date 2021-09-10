/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_abstract_output.h"

#include <QObject>
#include <QRect>

namespace KWin
{

class SoftwareVsyncMonitor;
class VirtualBackend;

class DrmVirtualOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    DrmVirtualOutput(DrmGpu *gpu);
    ~DrmVirtualOutput() override;

    bool present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion) override;
    GbmBuffer *currentBuffer() const override;
    QSize sourceSize() const override;
    bool isDpmsEnabled() const override;

    bool isFormatSupported(uint32_t drmFormat) const override;
    QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const override;

    int gammaRampSize() const override {
        return 200;
    }
    bool setGammaRamp(const GammaRamp &gamma) override {
        Q_UNUSED(gamma);
        return true;
    }
    bool needsSoftwareTransformation() const override {
        return false;
    }

private:
    void vblank(std::chrono::nanoseconds timestamp);
    void applyMode(int modeIndex) override;
    void updateMode(const QSize &size, uint32_t refreshRate) override;
    void setDpmsMode(DpmsMode mode) override;
    void updateEnablement(bool enable) override;

    QSharedPointer<DrmBuffer> m_currentBuffer;
    bool m_pageFlipPending = true;
    int m_modeIndex = 0;
    bool m_dpmsEnabled = true;

    int m_identifier;
    SoftwareVsyncMonitor *m_vsyncMonitor;
};

}
