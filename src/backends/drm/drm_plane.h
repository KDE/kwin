/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_object.h"

#include <QMap>
#include <QPoint>
#include <QSize>
#include <memory>
#include <qobjectdefs.h>

namespace KWin
{

class DrmFramebuffer;
class DrmCrtc;

class DrmPlane : public DrmObject
{
    Q_GADGET
public:
    DrmPlane(DrmGpu *gpu, uint32_t planeId);

    bool updateProperties() override;
    void disable(DrmAtomicCommit *commit) override;

    bool isCrtcSupported(int pipeIndex) const;
    QMap<uint32_t, QVector<uint64_t>> formats() const;

    std::shared_ptr<DrmFramebuffer> current() const;
    std::shared_ptr<DrmFramebuffer> next() const;
    void setCurrent(const std::shared_ptr<DrmFramebuffer> &b);
    void setNext(const std::shared_ptr<DrmFramebuffer> &b);
    void flipBuffer();

    void set(DrmAtomicCommit *commit, const QPoint &srcPos, const QSize &srcSize, const QRect &dst);

    void releaseBuffers();

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

    static int32_t transformationToDegrees(Transformations transformation);

private:
    std::shared_ptr<DrmFramebuffer> m_current;
    std::shared_ptr<DrmFramebuffer> m_next;

    QMap<uint32_t, QVector<uint64_t>> m_supportedFormats;
    uint32_t m_possibleCrtcs;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmPlane::Transformations)
