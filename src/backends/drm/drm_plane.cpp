/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_plane.h"

#include "config-kwin.h"

#include "drm_atomic_commit.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_pointer.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmPlane::DrmPlane(DrmGpu *gpu, uint32_t planeId)
    : DrmObject(gpu, planeId, DRM_MODE_OBJECT_PLANE)
    , type(this, QByteArrayLiteral("type"), {
                                                QByteArrayLiteral("Overlay"),
                                                QByteArrayLiteral("Primary"),
                                                QByteArrayLiteral("Cursor"),
                                            })
    , srcX(this, QByteArrayLiteral("SRC_X"))
    , srcY(this, QByteArrayLiteral("SRC_Y"))
    , srcW(this, QByteArrayLiteral("SRC_W"))
    , srcH(this, QByteArrayLiteral("SRC_H"))
    , crtcX(this, QByteArrayLiteral("CRTC_X"))
    , crtcY(this, QByteArrayLiteral("CRTC_Y"))
    , crtcW(this, QByteArrayLiteral("CRTC_W"))
    , crtcH(this, QByteArrayLiteral("CRTC_H"))
    , fbId(this, QByteArrayLiteral("FB_ID"))
    , crtcId(this, QByteArrayLiteral("CRTC_ID"))
    , rotation(this, QByteArrayLiteral("rotation"), {
                                                        QByteArrayLiteral("rotate-0"),
                                                        QByteArrayLiteral("rotate-90"),
                                                        QByteArrayLiteral("rotate-180"),
                                                        QByteArrayLiteral("rotate-270"),
                                                        QByteArrayLiteral("reflect-x"),
                                                        QByteArrayLiteral("reflect-y"),
                                                    })
    , inFormats(this, QByteArrayLiteral("IN_FORMATS"))
{
}

bool DrmPlane::updateProperties()
{
    DrmUniquePtr<drmModePlane> p(drmModeGetPlane(gpu()->fd(), id()));
    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << id();
        return false;
    }
    DrmPropertyList props = queryProperties();
    type.update(props);
    srcX.update(props);
    srcY.update(props);
    srcW.update(props);
    srcH.update(props);
    crtcX.update(props);
    crtcY.update(props);
    crtcW.update(props);
    crtcH.update(props);
    fbId.update(props);
    crtcId.update(props);
    rotation.update(props);
    inFormats.update(props);

    if (!type.isValid() || !srcX.isValid() || !srcY.isValid() || !srcW.isValid() || !srcH.isValid()
        || !crtcX.isValid() || !crtcY.isValid() || !crtcW.isValid() || !crtcH.isValid() || !fbId.isValid()) {
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    // read formats from blob if available and if modifiers are supported, and from the plane object if not
    if (inFormats.isValid() && inFormats.immutableBlob() && gpu()->addFB2ModifiersSupported()) {
        drmModeFormatModifierIterator iterator{};
        while (drmModeFormatModifierBlobIterNext(inFormats.immutableBlob(), &iterator)) {
            m_supportedFormats[iterator.fmt].push_back(iterator.mod);
        }
    } else {
        for (uint32_t i = 0; i < p->count_formats; i++) {
            m_supportedFormats.insert(p->formats[i], {DRM_FORMAT_MOD_LINEAR});
        }
    }
    if (m_supportedFormats.isEmpty()) {
        qCWarning(KWIN_DRM) << "Driver doesn't advertise any formats for this plane. Falling back to XRGB8888 without explicit modifiers";
        m_supportedFormats.insert(DRM_FORMAT_XRGB8888, {});
    }
    return true;
}

void DrmPlane::setNext(const std::shared_ptr<DrmFramebuffer> &b)
{
    m_next = b;
}

void DrmPlane::flipBuffer()
{
    m_current = m_next;
    m_next = nullptr;
}

void DrmPlane::set(DrmAtomicCommit *commit, const QPoint &srcPos, const QSize &srcSize, const QRect &dst)
{
    // Src* are in 16.16 fixed point format
    commit->addProperty(srcX, srcPos.x() << 16);
    commit->addProperty(srcX, srcPos.x() << 16);
    commit->addProperty(srcY, srcPos.y() << 16);
    commit->addProperty(srcW, srcSize.width() << 16);
    commit->addProperty(srcH, srcSize.height() << 16);
    commit->addProperty(crtcX, dst.x());
    commit->addProperty(crtcY, dst.y());
    commit->addProperty(crtcW, dst.width());
    commit->addProperty(crtcH, dst.height());
}

bool DrmPlane::isCrtcSupported(int pipeIndex) const
{
    return (m_possibleCrtcs & (1 << pipeIndex));
}

QMap<uint32_t, QVector<uint64_t>> DrmPlane::formats() const
{
    return m_supportedFormats;
}

std::shared_ptr<DrmFramebuffer> DrmPlane::current() const
{
    return m_current;
}

std::shared_ptr<DrmFramebuffer> DrmPlane::next() const
{
    return m_next;
}

void DrmPlane::setCurrent(const std::shared_ptr<DrmFramebuffer> &b)
{
    m_current = b;
}

void DrmPlane::disable(DrmAtomicCommit *commit)
{
    commit->addProperty(crtcId, 0);
    commit->addProperty(fbId, 0);
    m_next = nullptr;
}

void DrmPlane::releaseBuffers()
{
    if (m_next) {
        m_next->releaseBuffer();
    }
    if (m_current) {
        m_current->releaseBuffer();
    }
}

int32_t DrmPlane::transformationToDegrees(DrmPlane::Transformations transformation)
{
    if (transformation & DrmPlane::Transformation::Rotate0) {
        return 0;
    } else if (transformation & DrmPlane::Transformation::Rotate90) {
        return 90;
    } else if (transformation & DrmPlane::Transformation::Rotate180) {
        return 180;
    } else {
        return 270;
    }
}
}
