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
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_pointer.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmPlane::DrmPlane(DrmGpu *gpu, uint32_t planeId)
    : DrmObject(gpu, planeId, {
                                  PropertyDefinition(QByteArrayLiteral("type"), Requirement::Required, {QByteArrayLiteral("Overlay"), QByteArrayLiteral("Primary"), QByteArrayLiteral("Cursor")}),
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
                                  PropertyDefinition(QByteArrayLiteral("rotation"), Requirement::Optional, {QByteArrayLiteral("rotate-0"), QByteArrayLiteral("rotate-90"), QByteArrayLiteral("rotate-180"), QByteArrayLiteral("rotate-270"), QByteArrayLiteral("reflect-x"), QByteArrayLiteral("reflect-y")}),
                                  PropertyDefinition(QByteArrayLiteral("IN_FORMATS"), Requirement::Optional),
                              },
                DRM_MODE_OBJECT_PLANE)
{
}

bool DrmPlane::init()
{
    DrmUniquePtr<drmModePlane> p(drmModeGetPlane(gpu()->fd(), id()));

    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << id();
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    bool success = initProps();
    if (success) {
        if (const auto prop = getProp(PropertyIndex::Rotation)) {
            m_supportedTransformations = Transformations();
            auto checkSupport = [this, prop](Transformation t) {
                if (prop->hasEnum(t)) {
                    m_supportedTransformations |= t;
                }
            };
            checkSupport(Transformation::Rotate0);
            checkSupport(Transformation::Rotate90);
            checkSupport(Transformation::Rotate180);
            checkSupport(Transformation::Rotate270);
            checkSupport(Transformation::ReflectX);
            checkSupport(Transformation::ReflectY);
        } else {
            m_supportedTransformations = Transformation::Rotate0;
        }

        // read formats from blob if available and if modifiers are supported, and from the plane object if not
        if (const auto formatProp = getProp(PropertyIndex::In_Formats); formatProp && formatProp->immutableBlob() && gpu()->addFB2ModifiersSupported()) {
            drmModeFormatModifierIterator iterator{};
            while (drmModeFormatModifierBlobIterNext(formatProp->immutableBlob(), &iterator)) {
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
    }
    return success;
}

DrmPlane::TypeIndex DrmPlane::type() const
{
    const auto &prop = getProp(PropertyIndex::Type);
    return prop->enumForValue<DrmPlane::TypeIndex>(prop->current());
}

void DrmPlane::setNext(const std::shared_ptr<DrmFramebuffer> &b)
{
    m_next = b;
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

void DrmPlane::set(const QPoint &srcPos, const QSize &srcSize, const QRect &dst)
{
    // Src* are in 16.16 fixed point format
    setPending(PropertyIndex::SrcX, srcPos.x() << 16);
    setPending(PropertyIndex::SrcY, srcPos.y() << 16);
    setPending(PropertyIndex::SrcW, srcSize.width() << 16);
    setPending(PropertyIndex::SrcH, srcSize.height() << 16);
    setPending(PropertyIndex::CrtcX, dst.x());
    setPending(PropertyIndex::CrtcY, dst.y());
    setPending(PropertyIndex::CrtcW, dst.width());
    setPending(PropertyIndex::CrtcH, dst.height());
}

void DrmPlane::setBuffer(DrmFramebuffer *buffer)
{
    setPending(PropertyIndex::FbId, buffer ? buffer->framebufferId() : 0);
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

DrmPlane::Transformations DrmPlane::supportedTransformations() const
{
    return m_supportedTransformations;
}

void DrmPlane::disable()
{
    setPending(PropertyIndex::CrtcId, 0);
    setPending(PropertyIndex::FbId, 0);
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
}
