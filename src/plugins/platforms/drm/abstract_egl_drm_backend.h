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
class DrmOutput;
class DrmBuffer;

class AbstractEglDrmBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    virtual int screenCount() const = 0;
    virtual bool addOutput(DrmOutput *output) = 0;
    virtual void removeOutput(DrmOutput *output) = 0;
    virtual bool swapBuffers(DrmOutput *output, const QRegion &dirty) {
        Q_UNUSED(output)
        Q_UNUSED(dirty)
        return false;
    }
    virtual bool exportFramebuffer(DrmOutput *output, void *data, const QSize &size, uint32_t stride) {
        Q_UNUSED(output)
        Q_UNUSED(data)
        Q_UNUSED(size)
        Q_UNUSED(stride)
        return false;
    }
    virtual int exportFramebufferAsDmabuf(DrmOutput *output, uint32_t *format, uint32_t *stride) {
        Q_UNUSED(output)
        Q_UNUSED(format)
        Q_UNUSED(stride)
        return 0;
    }
    virtual QRegion beginFrameForSecondaryGpu(DrmOutput *output) {
        Q_UNUSED(output)
        return QRegion();
    }

    DrmGpu *gpu() const {
        return m_gpu;
    }

    virtual QSharedPointer<DrmBuffer> renderTestFrame(DrmOutput *output) = 0;

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
