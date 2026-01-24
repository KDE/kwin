/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorspace.h"
#include "eglswapchain.h"
#include "kwin_export.h"
#include "utils/damagejournal.h"

#include <QObject>

namespace KWin
{

class GraphicsBuffer;
class RenderDevice;

class KWIN_EXPORT EglMultiGpuSwapchain : public QObject
{
    Q_OBJECT
public:
    explicit EglMultiGpuSwapchain(RenderDevice *sourceDevice,
                                  const std::shared_ptr<EglContext> &targetContext);
    ~EglMultiGpuSwapchain();

    std::pair<GraphicsBuffer *, std::shared_ptr<ColorDescription>> importBuffer(GraphicsBuffer *buffer,
                                                                                const std::shared_ptr<ColorDescription> &color);

private:
    void handleDeviceRemoved(RenderDevice *device);

    RenderDevice *m_sourceDevice = nullptr;
    std::shared_ptr<EglContext> m_sourceContext;
    const std::shared_ptr<EglContext> m_importContext;
    std::shared_ptr<EglSwapchain> m_swapchain;
    DamageJournal m_journal;
    std::optional<uint32_t> m_lastFormat;
};

}
