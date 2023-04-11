/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021-2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_property.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_object.h"
#include <cerrno>

namespace KWin
{

DrmProperty::DrmProperty(DrmObject *obj, const QByteArray &name, const QVector<QByteArray> &enumNames)
    : m_obj(obj)
    , m_propName(name)
    , m_enumNames(enumNames)
{
}

bool DrmProperty::setPropertyLegacy(uint64_t value)
{
    if (m_current == value) {
        return true;
    } else if (drmModeObjectSetProperty(m_obj->gpu()->fd(), m_obj->id(), m_obj->type(), m_propId, value) == 0) {
        m_current = value;
        return true;
    } else {
        return false;
    }
}

void DrmProperty::update(DrmPropertyList &propertyList)
{
    if (const auto opt = propertyList.takeProperty(m_propName)) {
        const auto &[prop, value] = *opt;
        m_propId = prop->prop_id;
        m_current = value;
        m_immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
        m_isBlob = prop->flags & DRM_MODE_PROP_BLOB;
        m_isBitmask = prop->flags & DRM_MODE_PROP_BITMASK;
        if (prop->flags & DRM_MODE_PROP_RANGE) {
            Q_ASSERT(prop->count_values > 1);
            m_minValue = prop->values[0];
            m_maxValue = prop->values[1];
        }
        m_enumToPropertyMap.clear();
        m_propertyToEnumMap.clear();
        // bitmasks need translation too, not just enums
        if (prop->flags & (DRM_MODE_PROP_ENUM | DRM_MODE_PROP_BITMASK)) {
            for (int i = 0; i < prop->count_enums; i++) {
                struct drm_mode_property_enum *en = &prop->enums[i];
                int j = m_enumNames.indexOf(QByteArray(en->name));
                if (j >= 0) {
                    if (m_isBitmask) {
                        m_enumToPropertyMap[1 << j] = 1 << en->value;
                        m_propertyToEnumMap[1 << en->value] = 1 << j;
                    } else {
                        m_enumToPropertyMap[j] = en->value;
                        m_propertyToEnumMap[en->value] = j;
                    }
                } else {
                    qCWarning(KWIN_DRM, "%s has unrecognized enum '%s'", qPrintable(m_propName), en->name);
                }
            }
        }
        if (m_immutable && m_isBlob) {
            if (m_current != 0) {
                m_immutableBlob.reset(drmModeGetPropertyBlob(m_obj->gpu()->fd(), m_current));
                if (m_immutableBlob && (!m_immutableBlob->data || !m_immutableBlob->length)) {
                    m_immutableBlob.reset();
                }
            } else {
                m_immutableBlob.reset();
            }
        }
    } else {
        m_propId = 0;
        m_immutableBlob.reset();
        m_enumToPropertyMap.clear();
        m_propertyToEnumMap.clear();
    }
}

uint64_t DrmProperty::value() const
{
    return m_current;
}

bool DrmProperty::hasAllEnums() const
{
    return m_enumToPropertyMap.count() == m_enumNames.count();
}

uint32_t DrmProperty::propId() const
{
    return m_propId;
}

const QByteArray &DrmProperty::name() const
{
    return m_propName;
}

bool DrmProperty::isImmutable() const
{
    return m_immutable;
}

bool DrmProperty::isBitmask() const
{
    return m_isBitmask;
}

uint64_t DrmProperty::minValue() const
{
    return m_minValue;
}

uint64_t DrmProperty::maxValue() const
{
    return m_maxValue;
}

drmModePropertyBlobRes *DrmProperty::immutableBlob() const
{
    return m_immutableBlob.get();
}

DrmObject *DrmProperty::drmObject() const
{
    return m_obj;
}

bool DrmProperty::isValid() const
{
    return m_propId != 0;
}
}
