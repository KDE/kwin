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


namespace KWin
{

class DrmBackend;
class DrmOutput;

class DrmObject
{
public:
    /**
     * Create DRM object representation.
     * @param object_id provided by the kernel
     * @param fd of the DRM device
     */
    DrmObject(uint32_t object_id, int fd);
    virtual ~DrmObject();

    /**
     * Must be called to query necessary data directly after creation.
     * @return true when initializing was successful
     */
    virtual bool atomicInit() = 0;

    uint32_t id() const {
        return m_id;
    }

    DrmOutput *output() const {
        return m_output;
    }
    void setOutput(DrmOutput* output) {
        m_output = output;
    }

    int fd() const {
        return m_fd;
    }

    /**
     * Populate an atomic request with data of this object.
     * @param req the atomic request
     * @return true when the request was successfully populated
     */
    virtual bool atomicPopulate(drmModeAtomicReq *req) const;

    void setValue(int prop, uint64_t new_value);
    bool propHasEnum(int prop, uint64_t value) const;

protected:
    /**
     * Initialize properties of object. Only derived classes know names and quantities of
     * properties.
     *
     * @return true when properties have been initialized successfully
     */
    virtual bool initProps() = 0;

    void setPropertyNames(QVector<QByteArray> &&vector);
    void initProp(int n, drmModeObjectProperties *properties,
                  QVector<QByteArray> enumNames = QVector<QByteArray>(0));

    bool doAtomicPopulate(drmModeAtomicReq *req, int firstProperty) const;

    class Property;
    bool atomicAddProperty(drmModeAtomicReq *req, Property *property) const;

    int m_fd;
    const uint32_t m_id;
    DrmOutput *m_output = nullptr;

    // for comparison with received name of DRM object
    QVector<Property *> m_props;

    class Property
    {
    public:
        Property(drmModePropertyRes *prop, uint64_t val, QVector<QByteArray> enumNames);
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

    private:
        uint32_t m_propId = 0;
        QByteArray m_propName;

        uint64_t m_value = 0;
        QVector<uint64_t> m_enumMap;
        QVector<QByteArray> m_enumNames;
    };

private:
    QVector<QByteArray> m_propsNames;
};

}

QDebug& operator<<(QDebug& stream, const KWin::DrmObject*);
