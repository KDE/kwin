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

DrmProperty::DrmProperty(DrmObject *obj, const QByteArray &name, const QList<QByteArray> &enumNames)
    : m_obj(obj)
    , m_propName(name)
    , m_enumNames(enumNames)
{
}

bool DrmProperty::setPropertyLegacy(uint64_t value)
{
    if (m_current == value) {
        return true;
    }

    const int ret = drmModeObjectSetProperty(m_obj->gpu()->fd(), m_obj->id(), m_obj->type(), m_propId, value);
    if (ret == 0) {
        m_current = value;
        return true;
    }

    qCWarning(KWIN_DRM) << "Failed to set property:" << m_propName << "for object ID:" << m_obj->id() << "to value:" << value << "error:" << strerror(-ret);
    return false;
}

void DrmProperty::update(DrmPropertyList &propertyList)
{
    if (const auto opt = propertyList.takeProperty(m_propName)) {
        const auto &[prop, value] = *opt;
        m_propId = prop->prop_id;
        m_current = value;
        m_flags = prop->flags;
        if ((prop->flags & DRM_MODE_PROP_RANGE) || (prop->flags & DRM_MODE_PROP_SIGNED_RANGE)) {
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
                    if (m_flags & DRM_MODE_PROP_BITMASK) {
                        m_enumToPropertyMap[1 << j] = 1 << en->value;
                        m_propertyToEnumMap[1 << en->value] = 1 << j;
                    } else {
                        m_enumToPropertyMap[j] = en->value;
                        m_propertyToEnumMap[en->value] = j;
                    }
                }
            }
        }
        m_enumValues.clear();
        if (prop->flags & DRM_MODE_PROP_ENUM) {
            QList<QString> names;
            for (const drm_mode_property_enum &en : std::span(prop->enums, prop->count_enums)) {
                m_enumValues.push_back(en.value);
                names.push_back(en.name);
            }
        }
        if ((m_flags & DRM_MODE_PROP_IMMUTABLE) && (m_flags & DRM_MODE_PROP_BLOB)) {
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

QList<uint64_t> DrmProperty::possibleEnumValues() const
{
    return m_enumValues;
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
    return m_flags & DRM_MODE_PROP_IMMUTABLE;
}

bool DrmProperty::isBitmask() const
{
    return m_flags & DRM_MODE_PROP_BITMASK;
}

uint64_t DrmProperty::maxValue() const
{
    return m_maxValue;
}

uint64_t DrmProperty::minValue() const
{
    return m_minValue;
}

void DrmProperty::checkValueInRange(uint64_t value) const
{
    if (Q_UNLIKELY(m_flags & DRM_MODE_PROP_RANGE && (value > m_maxValue || value < m_minValue))) {
        qCWarning(KWIN_DRM) << "Range property value out of bounds." << m_propName << " value:" << value << "min:" << m_minValue << "max:" << m_maxValue;
    }
    if (Q_UNLIKELY(m_flags & DRM_MODE_PROP_SIGNED_RANGE && (int64_t(value) > int64_t(m_maxValue) || int64_t(value) < int64_t(m_minValue)))) {
        qCWarning(KWIN_DRM) << "Signed range property value out of bounds." << m_propName << " value:" << int64_t(value) << "min:" << int64_t(m_minValue) << "max:" << int64_t(m_maxValue);
    }
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
