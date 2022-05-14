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
    , m_pending(val)
    , m_next(val)
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

void DrmProperty::commit()
{
    if (m_immutable || m_current == m_pending) {
        return;
    }
    setCurrent(m_pending);
}

void DrmProperty::commitPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    m_next = m_pending;
}

void DrmProperty::rollbackPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    m_pending = m_next;
}

bool DrmProperty::setPropertyLegacy(uint64_t value)
{
    if (drmModeObjectSetProperty(m_obj->gpu()->fd(), m_obj->id(), m_obj->type(), m_propId, value) == 0) {
        m_current = m_next = m_pending = value;
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

void DrmProperty::setPending(uint64_t value)
{
    m_pending = value;
}

uint64_t DrmProperty::pending() const
{
    return m_pending;
}

bool DrmProperty::needsCommit() const
{
    return m_pending != m_current;
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

QString DrmProperty::valueString(uint64_t value) const
{
    if (m_isBitmask) {
        QString ret;
        bool first = true;
        for (uint64_t mask = 1; mask >= value && mask != 0; mask <<= 1) {
            if (value & mask) {
                if (!first) {
                    ret += " | ";
                }
                first = false;
                uint64_t enumValue = enumForValue<uint64_t>(mask);
                int enumIndex = 0;
                while (!(enumValue & (1ull << enumIndex)) && enumIndex < 64) {
                    enumIndex++;
                }
                if (enumIndex < m_enumNames.size()) {
                    ret += m_enumNames[enumIndex];
                }
            }
        }
        return ret;
    } else if (!m_enumNames.isEmpty()) {
        if (const uint64_t index = enumForValue<uint64_t>(value); index < (uint)m_enumNames.size()) {
            return m_enumNames[index];
        } else {
            return QStringLiteral("invalid value: %d").arg(value);
        }
    } else if (m_propName == QStringLiteral("SRC_X") || m_propName == QStringLiteral("SRC_Y") || m_propName == QStringLiteral("SRC_W") || m_propName == QStringLiteral("SRC_H")) {
        QString ret;
        ret.setNum(value / (float)(1ul << 16));
        return ret;
    } else {
        QString ret;
        ret.setNum(value);
        return ret;
    }
}
}
