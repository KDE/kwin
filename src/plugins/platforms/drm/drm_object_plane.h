/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_object.h"

#include <qobjectdefs.h>
#include <xf86drmMode.h>
#include <QSharedPointer>

namespace KWin
{

class DrmBuffer;

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
    TypeIndex type();

    bool isCrtcSupported(int pipeIndex) const {
        return (m_possibleCrtcs & (1 << pipeIndex));
    }
    QVector<uint32_t> formats() const {
        return m_formats;
    }

    QSharedPointer<DrmBuffer> current() const {
        return m_current;
    }
    QSharedPointer<DrmBuffer> next() const {
        return m_next;
    }
    void setCurrent(const QSharedPointer<DrmBuffer> &b) {
        m_current = b;
    }
    void setNext(const QSharedPointer<DrmBuffer> &b);
    void setTransformation(Transformations t);
    Transformations transformation();

    void flipBuffer();

    Transformations supportedTransformations() const {
        return m_supportedTransformations;
    }

    void setScaled(const QSize &srcSize, const QSize &modeSize, int crtcId, bool enable);
    void set(const QSize &src, const QPoint &dstPos, const QSize &dstSize, int crtcId, bool enable);

private:
    QSharedPointer<DrmBuffer> m_current;
    QSharedPointer<DrmBuffer> m_next;

    // TODO: See weston drm_output_check_plane_format for future use of these member variables
    QVector<uint32_t> m_formats;        // Possible formats, which can be presented on this plane

    // TODO: when using overlay planes in the future: restrict possible screens / crtcs of planes
    uint32_t m_possibleCrtcs;

    Transformations m_supportedTransformations = Transformation::Rotate0;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmPlane::Transformations)

