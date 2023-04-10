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

DrmProperty::DrmProperty(DrmObject *obj, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_current(val)
    , m_immutable(prop->flags & DRM_MODE_PROP_IMMUTABLE)
    , m_isBlob(prop->flags & DRM_MODE_PROP_BLOB)
    , m_isBitmask(prop->flags & DRM_MODE_PROP_BITMASK)
    , m_obj(obj)
{
    if (!enumNames.isEmpty()) {
        m_enumNames = enumNames;
        initEnumMap(prop);
    }
    if (prop->flags & DRM_MODE_PROP_RANGE) {
        Q_ASSERT(prop->count_values > 1);
        m_minValue = prop->values[0];
        m_maxValue = prop->values[1];
    }
    updateBlob();
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

void DrmProperty::initEnumMap(drmModePropertyRes *prop)
{
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

void DrmProperty::setCurrent(uint64_t value)
{
    m_current = value;
    updateBlob();
}

uint64_t DrmProperty::current() const
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

bool DrmProperty::isLegacy() const
{
    return m_legacy;
}

void DrmProperty::setLegacy()
{
    m_legacy = true;
}

uint64_t DrmProperty::minValue() const
{
    return m_minValue;
}

uint64_t DrmProperty::maxValue() const
{
    return m_maxValue;
}

void DrmProperty::updateBlob()
{
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
}

drmModePropertyBlobRes *DrmProperty::immutableBlob() const
{
    return m_immutableBlob.get();
}

const DrmObject *DrmProperty::drmObject() const
{
    return m_obj;
}
}
