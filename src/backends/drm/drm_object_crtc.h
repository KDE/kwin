/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_OBJECT_CRTC_H
#define KWIN_DRM_OBJECT_CRTC_H

#include "drm_object.h"

#include <QSharedPointer>
#include <QPoint>

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;
class GammaRamp;
class DrmGpu;
class DrmPlane;

class DrmCrtc : public DrmObject
{
public:
    DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane);

    enum class PropertyIndex : uint32_t {
        ModeId = 0,
        Active,
        VrrEnabled,
        Gamma_LUT,
        Gamma_LUT_Size,
        Count
    };

    bool init() override;
    bool needsModeset() const override;
    void disable() override;

    int pipeIndex() const;
    int gammaRampSize() const;
    DrmPlane *primaryPlane() const;
    DrmPlane *cursorPlane() const;
    drmModeModeInfo queryCurrentMode();

    QSharedPointer<DrmBuffer> current() const;
    QSharedPointer<DrmBuffer> next() const;
    void setCurrent(const QSharedPointer<DrmBuffer> &buffer);
    void setNext(const QSharedPointer<DrmBuffer> &buffer);
    void flipBuffer();

private:
    DrmScopedPointer<drmModeCrtc> m_crtc;
    QSharedPointer<DrmBuffer> m_currentBuffer;
    QSharedPointer<DrmBuffer> m_nextBuffer;
    int m_pipeIndex;
    DrmPlane *m_primaryPlane;
    DrmPlane *m_cursorPlane;
};

}

#endif

