/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <xf86drmMode.h>

#include <QVector>
#include <QMap>
#include <QByteArray>

namespace KWin
{

class DrmObject;

class DrmProperty
{
public:
    DrmProperty(DrmObject *obj, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames);
    virtual ~DrmProperty();

    void initEnumMap(drmModePropertyRes *prop);

    QVector<QByteArray> enumNames() const;
    bool hasEnum(uint64_t value) const;
    bool hasAllEnums() const;
    template <typename T>
    T enumForValue(uint64_t value) const {
        return static_cast<T>(m_enumMap[value]);
    }
    template <typename T>
    bool setEnum(T index) {
        if (hasEnum(static_cast<uint64_t>(index))) {
            setPending(m_enumMap[static_cast<uint32_t>(index)]);
            return true;
        }
        return false;
    }

    uint32_t propId() const;
    const QByteArray &name() const;
    bool isImmutable() const;
    bool isLegacy() const;
    /**
    * Makes this property be ignored by DrmObject::atomicPopulate
    */
    void setLegacy();

    void setPending(uint64_t value);
    uint64_t pending() const;

    void setCurrent(uint64_t value);
    uint64_t current() const;

    uint64_t minValue() const;
    uint64_t maxValue() const;

    void commit();
    void commitPending();
    void rollbackPending();
    bool needsCommit() const;

    bool setPropertyLegacy(uint64_t value);
    template<typename T>
    bool setEnumLegacy(T value) {
        if (hasEnum(static_cast<uint64_t>(value))) {
            return setPropertyLegacy(m_enumMap[static_cast<uint32_t>(value)]);
        }
        return false;
    }

private:
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

    uint64_t m_minValue = -1;
    uint64_t m_maxValue = -1;

    QMap<uint32_t, uint64_t> m_enumMap;
    QVector<QByteArray> m_enumNames;
    const bool m_immutable;
    bool m_legacy = false;
    const DrmObject *m_obj;
};

}
