/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021-2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "drm_pointer.h"
#include "drm_logging.h"

#include <QByteArray>
#include <QList>
#include <QMap>

#include <xf86drmMode.h>

namespace KWin
{

class DrmObject;
class DrmPropertyList;

class DrmProperty
{
public:
    explicit DrmProperty(DrmObject *obj, const QByteArray &name, const QList<QByteArray> &enumNames = {});

    const QByteArray &name() const;
    DrmObject *drmObject() const;

    uint32_t propId() const;
    bool isImmutable() const;
    bool isBitmask() const;
    bool hasAllEnums() const;
    uint64_t value() const;
    drmModePropertyBlobRes *immutableBlob() const;
    uint64_t minValue() const;
    uint64_t maxValue() const;
    bool isValid() const;

    /**
     * Prints a warning and returns false if @p value is unacceptable for the property
     */
    void checkValueInRange(uint64_t value) const;

    void update(DrmPropertyList &propertyList);
    bool setPropertyLegacy(uint64_t value);

    QList<uint64_t> possibleEnumValues() const;

protected:
    DrmObject *const m_obj;
    const QByteArray m_propName;
    const QList<QByteArray> m_enumNames;

    uint32_t m_propId = 0;
    // the last known value from the kernel
    uint64_t m_current = 0;
    DrmUniquePtr<drmModePropertyBlobRes> m_immutableBlob;

    uint64_t m_minValue = -1;
    uint64_t m_maxValue = -1;

    QMap<uint64_t, uint64_t> m_enumToPropertyMap;
    QMap<uint64_t, uint64_t> m_propertyToEnumMap;
    QList<uint64_t> m_enumValues;
    uint32_t m_flags;
};

template<typename Enum>
class DrmEnumProperty : public DrmProperty
{
public:
    DrmEnumProperty(DrmObject *obj, const QByteArray &name, const QList<QByteArray> &enumNames)
        : DrmProperty(obj, name, enumNames)
    {
    }

    Enum enumValue() const
    {
        return enumForValue(value());
    }

    bool hasEnum(Enum value) const
    {
        const uint64_t integerValue = static_cast<uint64_t>(value);
        if (isBitmask()) {
            for (uint64_t mask = 1; integerValue >= mask && mask != 0; mask <<= 1) {
                if ((integerValue & mask) && !m_enumToPropertyMap.contains(mask)) {
                    return false;
                }
            }
            return true;
        } else {
            return m_enumToPropertyMap.contains(integerValue);
        }
    }

    bool hasEnumForValue(uint64_t value) const
    {
        return m_propertyToEnumMap.contains(value);
    }

    Enum enumForValue(uint64_t value) const
    {
        if (isBitmask()) {
            uint64_t ret = 0;
            for (uint64_t mask = 1; value >= mask && mask != 0; mask <<= 1) {
                if (value & mask) {
                    ret |= m_propertyToEnumMap[mask];
                }
            }
            return static_cast<Enum>(ret);
        } else {
            return static_cast<Enum>(m_propertyToEnumMap[value]);
        }
    }

    uint64_t valueForEnum(Enum enumValue) const
    {
        const uint64_t integer = static_cast<uint64_t>(enumValue);
        if (isBitmask()) {
            uint64_t set = 0;
            for (uint64_t mask = 1; integer >= mask && mask != 0; mask <<= 1) {
                if (integer & mask) {
                    set |= m_enumToPropertyMap[mask];
                }
            }
            return set;
        } else {
            return m_enumToPropertyMap[integer];
        }
    }

    bool setEnumLegacy(Enum value)
    {
        if (hasEnum(value)) {
            return setPropertyLegacy(valueForEnum(value));
        } else {
            return false;
        }
    }
};
}
