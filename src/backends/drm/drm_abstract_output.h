/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_wayland_output.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class DrmBuffer;
class GbmBuffer;

class DrmAbstractOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:

    virtual bool initCursor(const QSize &cursorSize);
    virtual bool showCursor();
    virtual bool hideCursor();
    virtual bool updateCursor();
    virtual bool moveCursor();

    virtual bool present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion) = 0;

    virtual bool needsSoftwareTransformation() const = 0;
    virtual QSize sourceSize() const = 0;
    virtual bool isFormatSupported(uint32_t drmFormat) const = 0;
    virtual QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const = 0;

    DrmGpu *gpu() const;
    RenderLoop *renderLoop() const override;

protected:
    friend class DrmBackend;
    friend class DrmGpu;
    DrmAbstractOutput(DrmGpu *gpu);

    RenderLoop *m_renderLoop;
    DrmGpu *m_gpu;
};

}
