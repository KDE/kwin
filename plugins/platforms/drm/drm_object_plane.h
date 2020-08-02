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

namespace KWin
{

class DrmBuffer;

class DrmPlane : public DrmObject
{
    Q_GADGET
public:
    DrmPlane(uint32_t plane_id, int fd);

    ~DrmPlane() override;

    enum class PropertyIndex {
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

    enum class TypeIndex {
        Overlay = 0,
        Primary,
        Cursor,
        Count
    };
    Q_ENUM(TypeIndex)

    enum class Transformation {
        Rotate0     = 1 << 0,
        Rotate90    = 1 << 1,
        Rotate180   = 1 << 2,
        Rotate270   = 1 << 3,
        ReflectX    = 1 << 4,
        ReflectY    = 1 << 5
    };
    Q_ENUM(Transformation)
    Q_DECLARE_FLAGS(Transformations, Transformation);

    bool atomicInit() override;
    bool initProps() override;
    TypeIndex type();

    bool isCrtcSupported(int resIndex) const {
        return (m_possibleCrtcs & (1 << resIndex));
    }
    QVector<uint32_t> formats() const {
        return m_formats;
    }

    DrmBuffer *current() const {
        return m_current;
    }
    DrmBuffer *next() const {
        return m_next;
    }
    void setCurrent(DrmBuffer *b) {
        m_current = b;
    }
    void setNext(DrmBuffer *b);
    void setTransformation(Transformations t);
    Transformations transformation();

    void flipBuffer();
    void flipBufferWithDelete();

    Transformations supportedTransformations() const {
        return m_supportedTransformations;
    }

    bool atomicPopulate(drmModeAtomicReq *req) const override;

private:
    DrmBuffer *m_current = nullptr;
    DrmBuffer *m_next = nullptr;

    // TODO: See weston drm_output_check_plane_format for future use of these member variables
    QVector<uint32_t> m_formats;        // Possible formats, which can be presented on this plane

    // TODO: when using overlay planes in the future: restrict possible screens / crtcs of planes
    uint32_t m_possibleCrtcs;

    Transformations m_supportedTransformations = Transformation::Rotate0;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmPlane::Transformations)

