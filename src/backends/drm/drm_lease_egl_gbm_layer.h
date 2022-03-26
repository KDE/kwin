/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include <QSharedPointer>

namespace KWin
{

class EglGbmBackend;

class DrmLeaseEglGbmLayer : public DrmPipelineLayer
{
public:
    DrmLeaseEglGbmLayer(EglGbmBackend *backend, DrmPipeline *pipeline);

    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;

private:
    QSharedPointer<DrmBuffer> m_buffer;
};

}
