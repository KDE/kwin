/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object_plane.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_pointer.h"
#include "logging.h"
#include "config-kwin.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmPlane::DrmPlane(DrmGpu *gpu, uint32_t planeId)
    : DrmObject(gpu, planeId, {
        PropertyDefinition(QByteArrayLiteral("type"), Requirement::Required, {
            QByteArrayLiteral("Overlay"),
            QByteArrayLiteral("Primary"),
            QByteArrayLiteral("Cursor")}),
        PropertyDefinition(QByteArrayLiteral("SRC_X"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("SRC_Y"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("SRC_W"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("SRC_H"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("CRTC_X"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("CRTC_Y"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("CRTC_W"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("CRTC_H"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("FB_ID"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("CRTC_ID"), Requirement::Required),
        PropertyDefinition(QByteArrayLiteral("rotation"), Requirement::Optional, {
            QByteArrayLiteral("rotate-0"),
            QByteArrayLiteral("rotate-90"),
            QByteArrayLiteral("rotate-180"),
            QByteArrayLiteral("rotate-270"),
            QByteArrayLiteral("reflect-x"),
            QByteArrayLiteral("reflect-y")}),
        PropertyDefinition(QByteArrayLiteral("IN_FORMATS"), Requirement::Optional),
        }, DRM_MODE_OBJECT_PLANE)
{
}

bool DrmPlane::init()
{
    DrmScopedPointer<drmModePlane> p(drmModeGetPlane(gpu()->fd(), id()));

    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << id();
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    bool success = initProps();
    if (success) {
        m_supportedTransformations = Transformations();
        auto checkSupport = [this] (uint64_t value, Transformation t) {
            if (propHasEnum(PropertyIndex::Rotation, value)) {
                m_supportedTransformations |= t;
            }
        };
        checkSupport(0, Transformation::Rotate0);
        checkSupport(1, Transformation::Rotate90);
        checkSupport(2, Transformation::Rotate180);
        checkSupport(3, Transformation::Rotate270);
        checkSupport(4, Transformation::ReflectX);
        checkSupport(5, Transformation::ReflectY);

        // read formats from blob if available and if modifiers are supported, and from the plane object if not
        if (auto formatProp = getProp(PropertyIndex::In_Formats); formatProp && gpu()->addFB2ModifiersSupported()) {
            DrmScopedPointer<drmModePropertyBlobRes> propertyBlob(drmModeGetPropertyBlob(gpu()->fd(), formatProp->current()));
            if (propertyBlob && propertyBlob->data) {
                auto blob = static_cast<drm_format_modifier_blob*>(propertyBlob->data);
                auto modifiers = reinterpret_cast<drm_format_modifier*>(reinterpret_cast<uint8_t*>(blob) + blob->modifiers_offset);
                uint32_t *formatarr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(blob) + blob->formats_offset);

                for (uint32_t f = 0; f < blob->count_formats; f++) {
                    auto format = formatarr[f];
                    QVector<uint64_t> mods;
                    for (uint32_t m = 0; m < blob->count_modifiers; m++) {
                        auto modifier = &modifiers[m];
                        // The modifier advertisement blob is partitioned into groups of 64 formats
                        if (m < modifier->offset || m > modifier->offset + 63) {
                            continue;
                        }
                        if (!(modifier->formats & (1 << (f - modifier->offset)))) {
                            continue;
                        }
                        mods << modifier->modifier;
                    }
                    m_supportedFormats.insert(format, mods);
                }
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
    }
    return success;
}

DrmPlane::TypeIndex DrmPlane::type() const
{
    const auto &prop = getProp(PropertyIndex::Type);
    return prop->enumForValue<DrmPlane::TypeIndex>(prop->current());
}

void DrmPlane::setNext(const QSharedPointer<DrmBuffer> &b)
{
    m_next = b;
}

bool DrmPlane::setTransformation(Transformations t)
{
    if (m_supportedTransformations & t) {
        return setPending(PropertyIndex::Rotation, t);
    } else {
        return false;
    }
}

DrmPlane::Transformations DrmPlane::transformation()
{
    if (auto property = getProp(PropertyIndex::Rotation)) {
        return Transformations(static_cast<uint32_t>(property->pending()));
    }
    return Transformations(Transformation::Rotate0);
}

void DrmPlane::flipBuffer()
{
    m_current = m_next;
    m_next = nullptr;
}

void DrmPlane::set(const QPoint &srcPos, const QSize &srcSize, const QPoint &dstPos, const QSize &dstSize)
{
    // Src* are in 16.16 fixed point format
    setPending(PropertyIndex::SrcX, srcPos.x() << 16);
    setPending(PropertyIndex::SrcY, srcPos.y() << 16);
    setPending(PropertyIndex::SrcW, srcSize.width() << 16);
    setPending(PropertyIndex::SrcH, srcSize.height() << 16);
    setPending(PropertyIndex::CrtcX, dstPos.x());
    setPending(PropertyIndex::CrtcY, dstPos.y());
    setPending(PropertyIndex::CrtcW, dstSize.width());
    setPending(PropertyIndex::CrtcH, dstSize.height());
}

void DrmPlane::setBuffer(DrmBuffer *buffer)
{
    setPending(PropertyIndex::FbId, buffer ? buffer->bufferId() : 0);
}

bool DrmPlane::needsModeset() const
{
    if (!gpu()->atomicModeSetting() || type() == TypeIndex::Cursor) {
        return false;
    }
    auto rotation = getProp(PropertyIndex::Rotation);
    if (rotation && rotation->needsCommit()) {
        return true;
    }
    return getProp(PropertyIndex::CrtcId)->needsCommit();
}

bool DrmPlane::isCrtcSupported(int pipeIndex) const
{
    return (m_possibleCrtcs & (1 << pipeIndex));
}

QMap<uint32_t, QVector<uint64_t>> DrmPlane::formats() const
{
    return m_supportedFormats;
}

QSharedPointer<DrmBuffer> DrmPlane::current() const
{
    return m_current;
}

QSharedPointer<DrmBuffer> DrmPlane::next() const
{
    return m_next;
}

void DrmPlane::setCurrent(const QSharedPointer<DrmBuffer> &b)
{
    m_current = b;
}

DrmPlane::Transformations DrmPlane::supportedTransformations() const
{
    return m_supportedTransformations;
}

void DrmPlane::disable()
{
    setPending(PropertyIndex::CrtcId, 0);
    setPending(PropertyIndex::FbId, 0);
}

}
