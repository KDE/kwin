/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "outputlayer.h"

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
    virtual QSharedPointer<DrmBuffer> currentBuffer() const = 0;
    virtual QRect pixelGeometry() const;
    virtual bool checkTestBuffer();
    virtual bool hasDirectScanoutBuffer() const;
    virtual void pageFlipped();
};

class DrmOverlayLayer : public DrmPipelineLayer
{
public:
    bool needsCompositing() const;
    void setNeedsCompositing(bool needsCompositing);

private:
    bool m_needsCompositing = false;
};
}
