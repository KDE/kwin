/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_object.h"

#include <qobjectdefs.h>
#include <QSharedPointer>
#include <QMap>

namespace KWin
{

class DrmBuffer;
class DrmCrtc;

class DrmPlane : public DrmObject
{
    Q_GADGET
public:
    DrmPlane(DrmGpu *gpu, uint32_t planeId);

    enum class PropertyIndex : uint32_t {
        Type = 0,
        SrcX,
        SrcY,
        SrcW,
        SrcH,
        CrtcX,
        CrtcY,
        CrtcW,
        CrtcH,
        FbId,
        CrtcId,
        Rotation,
        In_Formats,
        Count
    };
    Q_ENUM(PropertyIndex)

    enum class TypeIndex : uint32_t {
        Overlay = 0,
        Primary,
        Cursor,
        Count
    };
    Q_ENUM(TypeIndex)

    enum class Transformation : uint32_t {
        Rotate0     = 1 << 0,
        Rotate90    = 1 << 1,
        Rotate180   = 1 << 2,
        Rotate270   = 1 << 3,
        ReflectX    = 1 << 4,
        ReflectY    = 1 << 5
    };
    Q_ENUM(Transformation)
    Q_DECLARE_FLAGS(Transformations, Transformation);

    bool init() override;
    bool needsModeset() const override;
    void disable() override;
    TypeIndex type() const;

    bool isCrtcSupported(int pipeIndex) const;
    QMap<uint32_t, QVector<uint64_t>> formats() const;

    QSharedPointer<DrmBuffer> current() const;
    QSharedPointer<DrmBuffer> next() const;
    void setCurrent(const QSharedPointer<DrmBuffer> &b);
    void setNext(const QSharedPointer<DrmBuffer> &b);
    void flipBuffer();

    void setBuffer(DrmBuffer *buffer);
    void set(const QPoint &srcPos, const QSize &srcSize, const QPoint &dstPos, const QSize &dstSize);

    bool setTransformation(Transformations t);
    Transformations transformation();
    Transformations supportedTransformations() const;

private:
    QSharedPointer<DrmBuffer> m_current;
    QSharedPointer<DrmBuffer> m_next;

    QMap<uint32_t, QVector<uint64_t>> m_supportedFormats;
    uint32_t m_possibleCrtcs;
    Transformations m_supportedTransformations = Transformation::Rotate0;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmPlane::Transformations)

