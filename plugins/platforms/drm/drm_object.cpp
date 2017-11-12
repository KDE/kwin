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
#include "drm_object.h"

#include "logging.h"

namespace KWin
{

/*
 * Defintions for class DrmObject
 */

DrmObject::DrmObject(uint32_t object_id, int fd)
    : m_fd(fd)
    , m_id(object_id)
{
}

DrmObject::~DrmObject()
{
    foreach(Property* p, m_props)
        delete p;
}

void DrmObject::setPropertyNames(QVector<QByteArray> &&vector)
{
    m_propsNames = std::move(vector);
    m_props.fill(nullptr, m_propsNames.size());
}

void DrmObject::initProp(int n, drmModeObjectProperties *properties, QVector<QByteArray> enumNames)
{
    for (unsigned int i = 0; i < properties->count_props; ++i) {
        drmModePropertyRes *prop = drmModeGetProperty(fd(), properties->props[i]);
        if (!prop) {
            continue;
        }
        if (prop->name == m_propsNames[n]) {
            qCDebug(KWIN_DRM).nospace() << m_id << ": " << prop->name << "' (id " << prop->prop_id
                              << "): " << properties->prop_values[i];
            m_props[n] = new Property(prop, properties->prop_values[i], enumNames);
        }
        drmModeFreeProperty(prop);
    }
}

bool DrmObject::atomicAddProperty(drmModeAtomicReq *req, Property *property)
{
    if (drmModeAtomicAddProperty(req, m_id, property->propId(), property->value()) <= 0) {
        qCWarning(KWIN_DRM) << "Adding property" << property->name() << "to atomic commit failed for object" << this;
        return false;
    }
    return true;
}

bool DrmObject::atomicPopulate(drmModeAtomicReq *req)
{
    bool ret = true;

    for (int i = 0; i < m_props.size(); i++) {
        auto property = m_props.at(i);
        if (!property) {
            continue;
        }
        ret &= atomicAddProperty(req, property);
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic plane" << m_id;
        return false;
    }
    return true;
}

/*
 * Defintions for struct Prop
 */

DrmObject::Property::Property(drmModePropertyRes *prop, uint64_t val, QVector<QByteArray> enumNames)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_value(val)
{
    if (!enumNames.isEmpty()) {
        qCDebug(KWIN_DRM) << m_propName << " has enums:" << enumNames;
        m_enumNames = enumNames;
        initEnumMap(prop);
    }
}

DrmObject::Property::~Property() = default;

void DrmObject::Property::initEnumMap(drmModePropertyRes *prop)
{
    if (!((prop->flags & DRM_MODE_PROP_ENUM) || (prop->flags & DRM_MODE_PROP_BITMASK)) || prop->count_enums < 1) {
        qCWarning(KWIN_DRM) << "Property '" << prop->name << "' ( id ="
                          << m_propId << ") should be enum valued, but it is not.";
        return;
    }

    int nameCount = m_enumNames.size();
    m_enumMap.resize(nameCount);

    qCDebug(KWIN_DRM).nospace() << "Test all " << prop->count_enums <<
                         " possible enums" <<":";

    for (int i = 0; i < prop->count_enums; i++) {

        struct drm_mode_property_enum *en = &prop->enums[i];
        int j = 0;

        while (QByteArray(en->name) != m_enumNames[j]) {
            j++;
            if (j == nameCount) {
                break;
            }
        }

        if (j == nameCount) {
            qCWarning(KWIN_DRM).nospace() << m_propName << " has unrecognized enum '" << en->name << "'";
        } else {
            qCDebug(KWIN_DRM).nospace() << "Enum '"
                              << en->name << "': runtime-value = " << en->value;
            m_enumMap[j] = en->value;
        }
    }

    if (KWIN_DRM().isDebugEnabled()) {
        for (int i = 0; i < m_enumMap.size(); i++) {
            if (m_value == m_enumMap[i]) {
                qCDebug(KWIN_DRM) << "=>" << m_propName << "with mapped enum value" << m_enumNames[i];
            }
        }
    }
}

}
