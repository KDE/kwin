/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_object.h"

#include <QPoint>
#include <memory>

namespace KWin
{

class DrmBackend;
class DrmFramebuffer;
class GammaRamp;
class DrmGpu;
class DrmPlane;

class DrmCrtc : public DrmObject
{
public:
    DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane);

    void disable(DrmAtomicCommit *commit) override;
    bool updateProperties() override;

    int pipeIndex() const;
    int gammaRampSize() const;
    DrmPlane *primaryPlane() const;
    DrmPlane *cursorPlane() const;
    drmModeModeInfo queryCurrentMode();

    std::shared_ptr<DrmFramebuffer> current() const;
    void setCurrent(const std::shared_ptr<DrmFramebuffer> &buffer);
    void releaseCurrentBuffer();

    DrmProperty modeId;
    DrmProperty active;
    DrmProperty vrrEnabled;
    DrmProperty gammaLut;
    DrmProperty gammaLutSize;
    DrmProperty ctm;
    DrmProperty degammaLut;
    DrmProperty degammaLutSize;

private:
    DrmUniquePtr<drmModeCrtc> m_crtc;
    std::shared_ptr<DrmFramebuffer> m_currentBuffer;
    int m_pipeIndex;
    DrmPlane *m_primaryPlane;
    DrmPlane *m_cursorPlane;
};

}
