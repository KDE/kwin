/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QVector>
#include <QByteArray>

// drm
#include <xf86drmMode.h>

#include "drm_pointer.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class DrmOutput;

class DrmObject
{
public:
    virtual ~DrmObject();

    /**
     * Must be called to query necessary data directly after creation.
     * @return true when initializing was successful
     */
    virtual bool init() = 0;

    uint32_t id() const {
        return m_id;
    }

    DrmGpu *gpu() const {
        return m_gpu;
    }

    /**
     * Populate an atomic request with data of this object.
     * @param req the atomic request
     * @return true when the request was successfully populated
     */
    bool atomicPopulate(drmModeAtomicReq *req) const;

    template <typename T>
    bool setPending(T prop, uint64_t new_value)
    {
        if (auto &property = m_props.at(static_cast<uint32_t>(prop))) {
            property->setPending(new_value);
            return true;
        }
        return false;
    }

    template <typename T>
    bool setPendingBlob(T prop, void *data, size_t length)
    {
        if (auto &property = m_props.at(static_cast<uint32_t>(prop))) {
            return property->setPendingBlob(data, length);
        }
        return false;
    }

    template <typename T>
    bool propHasEnum(T prop, uint64_t value) const
    {
        const auto &property = m_props.at(static_cast<uint32_t>(prop));
        return property ? property->hasEnum(value) : false;
    }

    class Property
    {
    public:
        Property(DrmGpu *gpu, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames, drmModePropertyBlobRes *blob);
        virtual ~Property();

        void initEnumMap(drmModePropertyRes *prop);

        QVector<QByteArray> enumNames() const {
            return m_enumNames;
        }
        QVector<uint64_t> enumMap() const {
            return m_enumMap;
        }
        bool hasEnum(uint64_t value) const {
            return m_enumMap.contains(value);
        }

        template <typename T>
        bool setEnum(T index) {
            if (hasEnum(static_cast<uint64_t>(index))) {
                setPending(m_enumMap[static_cast<uint32_t>(index)]);
                return true;
            }
            return false;
        }

        uint32_t propId() const {
            return m_propId;
        }
        const QByteArray &name() const {
            return m_propName;
        }
        bool isImmutable() const {
            return m_immutable;
        }
        bool isLegacy() const {
            return m_legacy;
        }
        /**
         * Makes this property be ignored by DrmObject::atomicPopulate
         */
        void setLegacy() {
            m_legacy = true;
        }

        void setPending(uint64_t value);
        uint64_t pending() const;
        bool setPendingBlob(void *blob, size_t length);
        drmModePropertyBlobRes *pendingBlob() const;

        void setCurrent(uint64_t value);
        uint64_t current() const;
        void setCurrentBlob(drmModePropertyBlobRes *blob);
        drmModePropertyBlobRes *currentBlob() const;

        void commit();
        void commitPending();
        void rollbackPending();
        bool needsCommit() const;

    private:
        uint32_t m_propId = 0;
        QByteArray m_propName;

        // the value that will be m_next after the property has been committed
        // has not necessarily been tested to work
        uint64_t m_pending = 0;
        drmModePropertyBlobRes *m_pendingBlob = nullptr;
        // the value that will be m_current after the next atomic commit
        // and has been tested to work
        uint64_t m_next = 0;
        drmModePropertyBlobRes *m_nextBlob = nullptr;
        // the value currently set for or by the kernel
        uint64_t m_current = 0;
        drmModePropertyBlobRes *m_currentBlob = nullptr;

        QVector<uint64_t> m_enumMap;
        QVector<QByteArray> m_enumNames;
        const bool m_immutable;
        bool m_legacy = false;
        const DrmGpu *m_gpu;
    };

    template <typename T>
    Property *getProp(T propIndex) const {
        return m_props[static_cast<uint32_t>(propIndex)];
    }

    QVector<Property*> properties();
    void commit();
    void commitPending();
    void rollbackPending();

    bool needsCommit() const;
    virtual bool needsModeset() const = 0;

    virtual bool updateProperties();

protected:
    enum class Requirement {
        Required,
        Optional
    };
    struct PropertyDefinition
    {
        PropertyDefinition(const QByteArray &name, Requirement requirement, const QVector<QByteArray> &&enumNames = {})
            : name(name)
            , requirement(requirement)
            , enumNames(enumNames)
        {
        }
        QByteArray name;
        Requirement requirement;
        QVector<QByteArray> enumNames;
    };

    DrmObject(DrmGpu *gpu, uint32_t objectId, const QVector<PropertyDefinition> &&vector, uint32_t objectType);

    bool initProps();

    template <typename T>
    void deleteProp(T prop)
    {
        delete m_props[static_cast<uint32_t>(prop)];
        m_props[static_cast<uint32_t>(prop)] = nullptr;
    }

    QVector<Property *> m_props;

private:
    DrmGpu *m_gpu;
    const uint32_t m_id;
    const uint32_t m_objectType;
    const QVector<PropertyDefinition> m_propertyDefinitions;
};

}

QDebug operator<<(QDebug stream, const KWin::DrmObject*);
