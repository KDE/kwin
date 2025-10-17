/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "drm_object.h"

#include <QMap>
#include <QPoint>
#include <QSize>
#include <memory>
#include <qobjectdefs.h>

namespace KWin
{

class DrmFramebuffer;
class DrmFramebufferData;
class DrmCrtc;

class DrmPlane : public DrmObject
{
    Q_GADGET
public:
    DrmPlane(DrmGpu *gpu, uint32_t planeId);

    bool init();

    bool updateProperties() override;
    void disable(DrmAtomicCommit *commit) override;

    bool isCrtcSupported(int pipeIndex) const;
    QHash<uint32_t, QList<uint64_t>> lowBandwidthFormats() const;
    QHash<uint32_t, QList<uint64_t>> formats() const;
    QHash<uint32_t, QList<uint64_t>> tearingFormats() const;
    bool supportsTransformation(OutputTransform transform) const;

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;
    void setCurrentBuffer(const std::shared_ptr<DrmFramebuffer> &b);
    void releaseCurrentBuffer();

    void set(DrmAtomicCommit *commit, const QRect &src, const QRect &dst);

    QList<QSize> recommendedSizes() const;

    enum class TypeIndex : uint64_t {
        Overlay = 0,
        Primary = 1,
        Cursor = 2
    };
    enum class Transformation : uint32_t {
        Rotate0 = 1 << 0,
        Rotate90 = 1 << 1,
        Rotate180 = 1 << 2,
        Rotate270 = 1 << 3,
        ReflectX = 1 << 4,
        ReflectY = 1 << 5
    };
    Q_ENUM(Transformation)
    Q_DECLARE_FLAGS(Transformations, Transformation)
    static Transformations outputTransformToPlaneTransform(OutputTransform transform);
    enum class PixelBlendMode : uint64_t {
        None,
        PreMultiplied,
        Coverage
    };
    enum class ColorEncoding : uint64_t {
        BT601_YCbCr,
        BT709_YCbCr,
        BT2020_YCbCr
    };
    enum class ColorRange : uint64_t {
        Limited_YCbCr,
        Full_YCbCr
    };

    DrmEnumProperty<TypeIndex> type;
    DrmProperty srcX;
    DrmProperty srcY;
    DrmProperty srcW;
    DrmProperty srcH;
    DrmProperty crtcX;
    DrmProperty crtcY;
    DrmProperty crtcW;
    DrmProperty crtcH;
    DrmProperty fbId;
    DrmProperty crtcId;
    DrmEnumProperty<Transformations> rotation;
    DrmProperty inFormats;
    DrmProperty alpha;
    DrmEnumProperty<PixelBlendMode> pixelBlendMode;
    DrmEnumProperty<ColorEncoding> colorEncoding;
    DrmEnumProperty<ColorRange> colorRange;
    DrmProperty vmHotspotX;
    DrmProperty vmHotspotY;
    DrmProperty inFenceFd;
    DrmProperty sizeHints;
    DrmProperty inFormatsForTearing;
    DrmProperty zpos;

private:
    std::shared_ptr<DrmFramebuffer> m_current;
    QList<std::shared_ptr<DrmFramebufferData>> m_lastBuffers;

    QHash<uint32_t, QList<uint64_t>> m_supportedFormats;
    QHash<uint32_t, QList<uint64_t>> m_lowBandwidthFormats;
    QHash<uint32_t, QList<uint64_t>> m_supportedTearingFormats;
    uint32_t m_possibleCrtcs = 0;
    QList<QSize> m_sizeHints;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmPlane::Transformations)
