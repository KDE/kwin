/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_DRM_OBJECT_H
#define KWIN_DRM_OBJECT_H

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
    // creates drm object by its id delivered by the kernel
    DrmObject(uint32_t object_id, int fd);

    virtual ~DrmObject();

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

    bool propHasEnum(int prop, uint64_t value) const {
        auto property = m_props.at(prop);
        return property ? property->hasEnum(value) : false;
    }

    void setValue(int prop, uint64_t new_value)
    {
        Q_ASSERT(prop < m_props.size());
        auto property = m_props.at(prop);
        if (property) {
            property->setValue(new_value);
        }
    }

    int fd() const {
        return m_fd;
    }

    virtual bool atomicPopulate(drmModeAtomicReq *req);

protected:
    virtual bool initProps() = 0;           // only derived classes know names and quantity of properties
    void setPropertyNames(QVector<QByteArray> &&vector);
    void initProp(int n, drmModeObjectProperties *properties, QVector<QByteArray> enumNames = QVector<QByteArray>(0));
    class Property;
    bool atomicAddProperty(drmModeAtomicReq *req, Property *property);

    int m_fd;
    const uint32_t m_id;
    DrmOutput *m_output = nullptr;

    // for comparision with received name of DRM object
    QVector<Property *> m_props;

    class Property
    {
    public:
        Property(drmModePropertyRes *prop, uint64_t val, QVector<QByteArray> enumNames);
        virtual ~Property();

        void initEnumMap(drmModePropertyRes *prop);

        uint64_t enumMap(int n) {
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

#endif

