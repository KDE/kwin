/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QMap>
#include <QPointer>

namespace KWin
{

class EglGbmBackend;
class DrmGpu;
class SurfaceInterface;

class DmabufFeedback
{
public:
    DmabufFeedback(DrmGpu *gpu, EglGbmBackend *eglBackend);

    void renderingSurface();
    void scanoutSuccessful(SurfaceInterface *surface);
    void scanoutFailed(SurfaceInterface *surface, const QMap<uint32_t, QList<uint64_t>> &formats);

private:
    QPointer<SurfaceInterface> m_surface;
    QMap<uint32_t, QList<uint64_t>> m_attemptedFormats;
    bool m_attemptedThisFrame = false;

    DrmGpu *const m_gpu;
    EglGbmBackend *const m_eglBackend;
};

}
