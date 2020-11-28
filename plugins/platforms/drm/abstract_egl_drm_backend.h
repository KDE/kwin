/*
 * KWin - the KDE window manager
 * This file is part of the KDE project.
 *
 * SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KWIN_ABSTRACTEGLDRMBACKEND_H
#define KWIN_ABSTRACTEGLDRMBACKEND_H

#include "abstract_egl_backend.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;

class AbstractEglDrmBackend : public AbstractEglBackend
{
public:
    ~AbstractEglDrmBackend();

    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    void screenGeometryChanged(const QSize &size) override;

    virtual int screenCount() const = 0;
    virtual void addSecondaryGpuOutput(AbstractOutput *output) {
        Q_UNUSED(output)
    }
    virtual int getDmabufForSecondaryGpuOutput(AbstractOutput *output, uint32_t *format, uint32_t *stride) {
        Q_UNUSED(output)
        Q_UNUSED(format)
        Q_UNUSED(stride)
        return 0;
    }
    virtual void cleanupDmabufForSecondaryGpuOutput(AbstractOutput *output) {
        Q_UNUSED(output)
    }
    virtual void removeSecondaryGpuOutput(AbstractOutput *output) {
        Q_UNUSED(output)
    }
    virtual QRegion beginFrameForSecondaryGpu(AbstractOutput *output) {
        Q_UNUSED(output)
        return QRegion();
    }
    virtual void renderFramebufferToSurface(AbstractOutput *output) {
        Q_UNUSED(output)
    }

    static AbstractEglDrmBackend *renderingBackend() {
        return static_cast<AbstractEglDrmBackend*>(primaryBackend());
    }

protected:
    AbstractEglDrmBackend(DrmBackend *drmBackend, DrmGpu *gpu);

    DrmBackend *m_backend;
    DrmGpu *m_gpu;

};

}

#endif // KWIN_ABSTRACTEGLDRMBACKEND_H
