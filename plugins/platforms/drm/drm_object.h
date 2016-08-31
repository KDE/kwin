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

class DrmOutput;

class DrmObject
{
public:
    // creates drm object by its id delivered by the kernel
    DrmObject(uint32_t object_id, int fd);

    virtual ~DrmObject() = 0;

    enum class AtomicReturn {
        NoChange,
        Success,
        Error
    };

    virtual bool init() = 0;

    uint32_t id() const {
        return m_id;
    }

    DrmOutput* output() const {
        return m_output;
    }
    void setOutput(DrmOutput* output) {
        m_output = output;
    }
    
    uint32_t propId(int index) {
        return m_props[index]->propId();
    }
    uint64_t propValue(int index) {
        return m_props[index]->value();
    }
    void setPropValue(int index, uint64_t new_value);

    uint32_t propsPending() {
        return m_propsPending;
    }
    uint32_t propsValid() {
        return m_propsValid;
    }
    void setPropsPending(uint32_t value) {
        m_propsPending = value;
    }
    void setPropsValid(uint32_t value) {
        m_propsValid = value;
    }

    bool atomicAddProperty(drmModeAtomicReq *req, int prop, uint64_t value);

protected:
    const int m_fd = 0;
    const uint32_t m_id = 0;

    DrmOutput *m_output = nullptr;

    QVector<QByteArray> m_propsNames;       // for comparision with received name of DRM object
    class Property;
    QVector<Property*> m_props;

    uint32_t m_propsPending = 0;
    uint32_t m_propsValid = 0;

    virtual bool initProps() = 0;           // only derived classes know names and quantity of properties
    void initProp(int n, drmModeObjectProperties *properties, QVector<QByteArray> enumNames = QVector<QByteArray>(0));

    class Property
    {
    public:
        Property(drmModePropertyRes *prop, uint64_t val, QVector<QByteArray> enumNames);
        virtual ~Property();

        void initEnumMap(drmModePropertyRes *prop);

        uint64_t enumMap(int n) {
            return m_enumMap[n];    // TODO: test on index out of bounds?
        }

        uint32_t propId() {
            return m_propId;
        }
        uint32_t value() {
            return m_value;
        }
        void setValue(uint64_t new_value) {
            m_value = new_value;
        }

    private:
        uint32_t m_propId = 0;
        QByteArray m_propName;

        uint64_t m_value = 0;
        QVector<uint64_t> m_enumMap;
        QVector<QByteArray> m_enumNames;
    };
};


}

#endif

