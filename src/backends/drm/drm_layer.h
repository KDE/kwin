/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "outputlayer.h"

#include <QObject>
#include <QRegion>
#include <QSharedPointer>
#include <optional>

namespace KWin
{

class SurfaceItem;
class DrmBuffer;
class GLTexture;
class DrmPipeline;

class DrmOutputLayer : public OutputLayer
{
public:
    virtual ~DrmOutputLayer();

    virtual QSharedPointer<GLTexture> texture() const;
    virtual QRegion currentDamage() const;
};

class DrmPipelineLayer : public DrmOutputLayer
{
public:
    DrmPipelineLayer(DrmPipeline *pipeline);

    virtual bool checkTestBuffer() = 0;
    virtual QSharedPointer<DrmBuffer> currentBuffer() const = 0;
    virtual bool hasDirectScanoutBuffer() const;

protected:
    DrmPipeline *const m_pipeline;
};

}
