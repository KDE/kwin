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
    /**
     * Create DRM object representation.
     * @param object_id provided by the kernel
     * @param fd of the DRM device
     */
    DrmObject(DrmGpu *gpu, uint32_t objectId);
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
    void setValue(T prop, uint64_t new_value)
    {
        if (auto &property = m_props.at(static_cast<uint32_t>(prop))) {
            property->setValue(new_value);
        }
    }

    template <typename T>
    bool propHasEnum(T prop, uint64_t value) const
    {
        const auto &property = m_props.at(static_cast<uint32_t>(prop));
        return property ? property->hasEnum(value) : false;
    }

protected:
    struct PropertyDefinition
    {
        PropertyDefinition(const QByteArray &name, const QVector<QByteArray> &&enumNames = {})
            : name(name), enumNames(enumNames)
        {
        }
        QByteArray name;
        QVector<QByteArray> enumNames;
    };
    /**
     * Initialize properties of object. Only derived classes know names and quantities of
     * properties.
     *
     * @return true when properties have been initialized successfully
     */
    bool initProps(const QVector<PropertyDefinition> &&vector, uint32_t objectType);

    template <typename T>
    void deleteProp(T prop)
    {
        delete m_props[static_cast<uint32_t>(prop)];
        m_props[static_cast<uint32_t>(prop)] = nullptr;
    }

    class Property;
    bool atomicAddProperty(drmModeAtomicReq *req, Property *property) const;

    // for comparison with received name of DRM object
    QVector<Property *> m_props;

    class Property
    {
    public:
        Property(drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames, drmModePropertyBlobRes *blob);
        virtual ~Property();

        void initEnumMap(drmModePropertyRes *prop);

        /**
         * For properties of enum type the enum map identifies the kernel runtime values,
         * which must be queried beforehand.
         *
         * @param n the index to the enum
         * @return the runtime enum value corresponding with enum index @param n
         */
        uint64_t enumMap(int n) const {
            return m_enumMap[n];    // TODO: test on index out of bounds?
        }
        bool hasEnum(uint64_t value) const {
            return m_enumMap.contains(value);
        }

        uint32_t propId() const {
            return m_propId;
        }
        uint64_t value() const {
            return m_value;
        }
        void setValue(uint64_t new_value) {
            m_value = new_value;
        }
        const QByteArray &name() const {
            return m_propName;
        }
        bool isImmutable() const {
            return m_immutable;
        }
        drmModePropertyBlobRes *blob() const {
            return m_blob.data();
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

    private:
        uint32_t m_propId = 0;
        QByteArray m_propName;

        uint64_t m_value = 0;
        QVector<uint64_t> m_enumMap;
        QVector<QByteArray> m_enumNames;
        const bool m_immutable;
        bool m_legacy = false;
        DrmScopedPointer<drmModePropertyBlobRes> m_blob;
    };

private:
    DrmGpu *m_gpu;
    QVector<QByteArray> m_propsNames;
    const uint32_t m_id;
};

}

QDebug operator<<(QDebug stream, const KWin::DrmObject*);
