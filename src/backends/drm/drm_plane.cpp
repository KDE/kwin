/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_plane.h"

#include "config-kwin.h"

#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_pointer.h"
#include "utils/drm_format_helper.h"

#include <drm_fourcc.h>
#include <ranges>
#include <span>

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
    , alpha(this, QByteArrayLiteral("alpha"))
    , pixelBlendMode(this, QByteArrayLiteral("pixel blend mode"), {
                                                                      QByteArrayLiteral("None"),
                                                                      QByteArrayLiteral("Pre-multiplied"),
                                                                      QByteArrayLiteral("Coverage"),
                                                                  })
    , colorEncoding(this, QByteArrayLiteral("COLOR_ENCODING"), {
                                                                   QByteArrayLiteral("ITU-R BT.601 YCbCr"),
                                                                   QByteArrayLiteral("ITU-R BT.709 YCbCr"),
                                                                   QByteArrayLiteral("ITU-R BT.2020 YCbCr"),
                                                               })
    , colorRange(this, QByteArrayLiteral("COLOR_RANGE"), {
                                                             QByteArrayLiteral("YCbCr limited range"),
                                                             QByteArrayLiteral("YCbCr full range"),
                                                         })
    , vmHotspotX(this, QByteArrayLiteral("HOTSPOT_X"))
    , vmHotspotY(this, QByteArrayLiteral("HOTSPOT_Y"))
    , inFenceFd(this, QByteArrayLiteral("IN_FENCE_FD"))
    , sizeHints(this, QByteArrayLiteral("SIZE_HINTS"))
    , inFormatsForTearing(this, QByteArrayLiteral("IN_FORMATS_ASYNC"))
    , zpos(this, QByteArrayLiteral("zpos"))
{
}

bool DrmPlane::init()
{
    return updateProperties();
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
    alpha.update(props);
    pixelBlendMode.update(props);
    colorEncoding.update(props);
    colorRange.update(props);
    vmHotspotX.update(props);
    vmHotspotY.update(props);
    inFenceFd.update(props);
    sizeHints.update(props);
    inFormatsForTearing.update(props);
    zpos.update(props);

    if (!type.isValid() || !srcX.isValid() || !srcY.isValid() || !srcW.isValid() || !srcH.isValid()
        || !crtcX.isValid() || !crtcY.isValid() || !crtcW.isValid() || !crtcH.isValid() || !fbId.isValid()) {
        qCWarning(KWIN_DRM) << "Failed to update the basic plane properties";
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    // read formats from blob if available and if modifiers are supported, and from the plane object if not
    m_supportedFormats.clear();
    if (inFormats.isValid() && inFormats.immutableBlob() && gpu()->addFB2ModifiersSupported()) {
        drmModeFormatModifierIterator iterator{};
        while (drmModeFormatModifierBlobIterNext(inFormats.immutableBlob(), &iterator)) {
            m_supportedFormats[iterator.fmt].push_back(iterator.mod);
        }
    } else {
        // if we don't have modifier support, assume the cursor needs a linear buffer
        const QList<uint64_t> modifiers = {type.enumValue() == TypeIndex::Cursor ? DRM_FORMAT_MOD_LINEAR : DRM_FORMAT_MOD_INVALID};
        for (uint32_t i = 0; i < p->count_formats; i++) {
            m_supportedFormats.insert(p->formats[i], modifiers);
        }
        if (m_supportedFormats.isEmpty()) {
            qCWarning(KWIN_DRM) << "Driver doesn't advertise any formats for this plane. Falling back to XRGB8888 without explicit modifiers";
            m_supportedFormats.insert(DRM_FORMAT_XRGB8888, modifiers);
        }
    }
    m_lowBandwidthFormats.clear();
    for (auto it = m_supportedFormats.begin(); it != m_supportedFormats.end(); it++) {
        const auto info = FormatInfo::get(it.key());
        if (info && info->bitsPerPixel <= 32) {
            if (it.value().contains(DRM_FORMAT_MOD_INVALID)) {
                // Mesa usually picks the modifier with lowest bandwidth requirements,
                // so prefer implicit modifiers for low bandwidth if supported
                m_lowBandwidthFormats.insert(it.key(), {DRM_FORMAT_MOD_INVALID});
            } else {
                m_lowBandwidthFormats.insert(it.key(), it.value());
            }
        }
    }

    m_sizeHints.clear();
    if (sizeHints.isValid() && sizeHints.immutableBlob()) {
        // TODO switch to drm_plane_size_hint once we require libdrm 2.4.122
        struct SizeHint
        {
            uint16_t width;
            uint16_t height;
        };
        std::span<SizeHint> hints(reinterpret_cast<SizeHint *>(sizeHints.immutableBlob()->data), sizeHints.immutableBlob()->length / sizeof(SizeHint));
        std::ranges::transform(hints, std::back_inserter(m_sizeHints), [](const SizeHint &hint) {
            return QSize(hint.width, hint.height);
        });
    }
    if (m_sizeHints.empty() && type.enumValue() == TypeIndex::Cursor) {
        m_sizeHints = {gpu()->cursorSize()};
    }

    if (inFormatsForTearing.isValid() && inFormatsForTearing.immutableBlob() && gpu()->addFB2ModifiersSupported()) {
        m_supportedTearingFormats.clear();
        drmModeFormatModifierIterator iterator{};
        while (drmModeFormatModifierBlobIterNext(inFormatsForTearing.immutableBlob(), &iterator)) {
            m_supportedTearingFormats[iterator.fmt].push_back(iterator.mod);
        }
    } else {
        m_supportedTearingFormats = m_supportedFormats;
    }
    return true;
}

void DrmPlane::set(DrmAtomicCommit *commit, const QRect &src, const QRect &dst)
{
    // Src* are in 16.16 fixed point format
    commit->addProperty(srcX, src.x() << 16);
    commit->addProperty(srcY, src.y() << 16);
    commit->addProperty(srcW, src.width() << 16);
    commit->addProperty(srcH, src.height() << 16);
    commit->addProperty(crtcX, dst.x());
    commit->addProperty(crtcY, dst.y());
    commit->addProperty(crtcW, dst.width());
    commit->addProperty(crtcH, dst.height());
}

bool DrmPlane::isCrtcSupported(int pipeIndex) const
{
    return (m_possibleCrtcs & (1 << pipeIndex));
}

QHash<uint32_t, QList<uint64_t>> DrmPlane::lowBandwidthFormats() const
{
    return m_lowBandwidthFormats;
}

QHash<uint32_t, QList<uint64_t>> DrmPlane::formats() const
{
    return m_supportedFormats;
}

QHash<uint32_t, QList<uint64_t>> DrmPlane::tearingFormats() const
{
    return m_supportedTearingFormats;
}

std::shared_ptr<DrmFramebuffer> DrmPlane::currentBuffer() const
{
    return m_current;
}

void DrmPlane::setCurrentBuffer(const std::shared_ptr<DrmFramebuffer> &b)
{
    if (m_current == b) {
        return;
    }

    m_current = b;
    if (b && !m_lastBuffers.contains(b->data())) {
        m_lastBuffers.prepend(b->data());
        m_lastBuffers.resize(4);
    }
}

void DrmPlane::disable(DrmAtomicCommit *commit)
{
    commit->addProperty(crtcId, 0);
    commit->addBuffer(this, nullptr, nullptr);
}

void DrmPlane::releaseCurrentBuffer()
{
    if (m_current) {
        m_current->releaseBuffer();
    }
}

DrmPlane::Transformations DrmPlane::outputTransformToPlaneTransform(OutputTransform transform)
{
    // note that drm transformations are counter clockwise
    switch (transform.kind()) {
    case OutputTransform::Kind::Normal:
        return Transformation::Rotate0;
    case OutputTransform::Kind::Rotate90:
        return Transformation::Rotate270;
    case OutputTransform::Kind::Rotate180:
        return Transformation::Rotate180;
    case OutputTransform::Kind::Rotate270:
        return Transformation::Rotate90;
    case OutputTransform::Kind::FlipY:
        return Transformation::Rotate0 | Transformation::ReflectY;
    case OutputTransform::Kind::FlipY90:
        return Transformation::Rotate270 | Transformation::ReflectY;
    case OutputTransform::Kind::FlipY180:
        return Transformation::Rotate180 | Transformation::ReflectY;
    case OutputTransform::Kind::FlipY270:
        return Transformation::Rotate90 | Transformation::ReflectY;
    }
    Q_UNREACHABLE();
}

bool DrmPlane::supportsTransformation(OutputTransform transform) const
{
    return rotation.isValid() && rotation.hasEnum(outputTransformToPlaneTransform(transform));
}

QList<QSize> DrmPlane::recommendedSizes() const
{
    return m_sizeHints;
}
}

#include "moc_drm_plane.cpp"
