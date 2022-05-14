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
#include <QMap>
#include <QVector>

#include <xf86drmMode.h>

namespace KWin
{

class DrmObject;

class DrmProperty
{
public:
    DrmProperty(DrmObject *obj, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames);

    bool hasAllEnums() const;

    template<typename Enum>
    bool hasEnum(Enum value) const
    {
        return m_enumToPropertyMap.contains(static_cast<uint64_t>(value));
    }
    template<typename T>
    T enumForValue(uint64_t value) const
    {
        if (m_isBitmask) {
            uint64_t ret = 0;
            for (uint64_t mask = 1; value >= mask && mask != 0; mask <<= 1) {
                if (value & mask) {
                    ret |= m_propertyToEnumMap[mask];
                }
            }
            return static_cast<T>(ret);
        } else {
            return static_cast<T>(m_propertyToEnumMap[value]);
        }
    }
    template<typename Enum>
    void setEnum(Enum value)
    {
        const uint64_t integer = static_cast<uint64_t>(value);
        if (m_isBitmask) {
            uint64_t set = 0;
            for (uint64_t mask = 1; integer >= mask && mask != 0; mask <<= 1) {
                if (integer & mask) {
                    set |= m_enumToPropertyMap[mask];
                }
            }
            setPending(set);
        } else {
            setPending(m_enumToPropertyMap[integer]);
        }
    }

    uint32_t propId() const;
    const QByteArray &name() const;
    bool isImmutable() const;
    bool isBitmask() const;
    bool isLegacy() const;
    /**
     * Makes this property be ignored by DrmObject::atomicPopulate
     */
    void setLegacy();

    void setPending(uint64_t value);
    uint64_t pending() const;

    void setCurrent(uint64_t value);
    uint64_t current() const;
    drmModePropertyBlobRes *immutableBlob() const;

    uint64_t minValue() const;
    uint64_t maxValue() const;

    void commit();
    void commitPending();
    void rollbackPending();
    bool needsCommit() const;

    bool setPropertyLegacy(uint64_t value);
    template<typename T>
    bool setEnumLegacy(T value)
    {
        if (hasEnum(static_cast<uint64_t>(value))) {
            return setPropertyLegacy(m_enumToPropertyMap[static_cast<uint32_t>(value)]);
        }
        return false;
    }

    QString valueString(uint64_t value) const;

private:
    void initEnumMap(drmModePropertyRes *prop);
    void updateBlob();

    uint32_t m_propId = 0;
    QByteArray m_propName;

    // the value that will be m_next after the property has been committed
    // has not necessarily been tested to work
    uint64_t m_pending = 0;
    // the value that will be m_current after the next atomic commit
    // and has been tested to work
    uint64_t m_next = 0;
    // the value currently set for or by the kernel
    uint64_t m_current = 0;
    DrmUniquePtr<drmModePropertyBlobRes> m_immutableBlob;

    uint64_t m_minValue = -1;
    uint64_t m_maxValue = -1;

    QMap<uint64_t, uint64_t> m_enumToPropertyMap;
    QMap<uint64_t, uint64_t> m_propertyToEnumMap;
    QVector<QByteArray> m_enumNames;
    const bool m_immutable;
    const bool m_isBlob;
    const bool m_isBitmask;
    bool m_legacy = false;
    const DrmObject *m_obj;
};

}
