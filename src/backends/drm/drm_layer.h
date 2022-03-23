/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

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

class DrmOutputLayer : public QObject
{
    Q_OBJECT
public:
    virtual ~DrmOutputLayer();

    virtual void aboutToStartPainting(const QRegion &damagedRegion);
    virtual std::optional<QRegion> startRendering();
    virtual bool endRendering(const QRegion &damagedRegion);

    /**
     * attempts to directly scan out the current buffer of the surfaceItem
     * @returns true if scanout was successful
     *          false if rendering is required
     */
    virtual bool scanout(SurfaceItem *surfaceItem);

    virtual QSharedPointer<GLTexture> texture() const;

    virtual QRegion currentDamage() const;
};

class DrmPipelineLayer : public DrmOutputLayer
{
public:
    DrmPipelineLayer(DrmPipeline *pipeline);

    /**
     * @returns a buffer for atomic test commits
     * If no fitting buffer is available, a new current buffer is created
     */
    virtual QSharedPointer<DrmBuffer> testBuffer() = 0;

    virtual QSharedPointer<DrmBuffer> currentBuffer() const = 0;
    virtual bool hasDirectScanoutBuffer() const;

protected:
    DrmPipeline *const m_pipeline;
};

}
