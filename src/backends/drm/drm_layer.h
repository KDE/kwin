/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <optional>
#include <QRegion>
#include <QSharedPointer>
#include <QObject>

namespace KWin
{

class SurfaceItem;
class DrmBuffer;
class DrmDisplayDevice;

class DrmLayer : public QObject
{
    Q_OBJECT
public:
    DrmLayer(DrmDisplayDevice *device);
    virtual ~DrmLayer();

    virtual std::optional<QRegion> startRendering() = 0;
    virtual bool endRendering(const QRegion &damagedRegion) = 0;

    /**
     * attempts to directly scan out the current buffer of the surfaceItem
     * @returns true if scanout was successful
     *          false if rendering is required
     */
    virtual bool scanout(SurfaceItem *surfaceItem) = 0;

    /**
     * @returns a buffer for atomic test commits
     * If no fitting buffer is available, a new current buffer is created
     */
    virtual QSharedPointer<DrmBuffer> testBuffer() = 0;

    virtual QSharedPointer<DrmBuffer> currentBuffer() const = 0;
    virtual QRegion currentDamage() const = 0;
    virtual bool hasDirectScanoutBuffer() const = 0;

    DrmDisplayDevice *displayDevice() const;

protected:
    DrmDisplayDevice *const m_displayDevice;
};

}
