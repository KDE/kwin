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

#include "drm_pointer.h"

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;
class GammaRamp;
class DrmGpu;

class DrmCrtc : public DrmObject
{
public:
    DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex);

    bool init() override;

    enum class PropertyIndex : uint32_t {
        ModeId = 0,
        Active = 1,
        Gamma_LUT = 2,
        Gamma_LUT_size = 3,
        Count
    };

    int pipeIndex() const {
        return m_pipeIndex;
    }

    QSharedPointer<DrmBuffer> current() {
        return m_currentBuffer;
    }
    QSharedPointer<DrmBuffer> next() {
        return m_nextBuffer;
    }
    void setCurrent(const QSharedPointer<DrmBuffer> &buffer) {
        m_currentBuffer = buffer;
    }
    void setNext(const QSharedPointer<DrmBuffer> &buffer) {
        m_nextBuffer = buffer;
    }

    void flipBuffer();

    int gammaRampSize() const {
        if (const auto &prop = m_props.at(static_cast<int>(PropertyIndex::Gamma_LUT_size))) {
            return prop->value();
        }
        return m_crtc->gamma_size;
    }

    bool hasGammaProp() const {
        return m_props.at(static_cast<int>(PropertyIndex::Gamma_LUT));
    }

    drmModeModeInfo queryCurrentMode();

private:
    DrmScopedPointer<drmModeCrtc> m_crtc;
    int m_pipeIndex;

    QSharedPointer<DrmBuffer> m_currentBuffer;
    QSharedPointer<DrmBuffer> m_nextBuffer;
};

}

#endif

