/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object.h"
#include "drm_pointer.h"

#include "logging.h"

namespace KWin
{

/*
 * Definitions for class DrmObject
 */

DrmObject::DrmObject(uint32_t object_id, int fd)
    : m_fd(fd)
    , m_id(object_id)
{
}

DrmObject::~DrmObject()
{
    for (auto *p : m_props) {
        delete p;
    }
}

void DrmObject::setPropertyNames(QVector<QByteArray> &&vector)
{
    m_propsNames = std::move(vector);
    m_props.fill(nullptr, m_propsNames.size());
}

void DrmObject::initProp(int n, drmModeObjectProperties *properties, QVector<QByteArray> enumNames)
{
    for (unsigned int i = 0; i < properties->count_props; ++i) {
        DrmScopedPointer<drmModePropertyRes> prop( drmModeGetProperty(fd(), properties->props[i]) );
        if (!prop) {
            qCWarning(KWIN_DRM) << "Getting property" << i << "failed";
            continue;
        }

        if (prop->name == m_propsNames[n]) {
            qCDebug(KWIN_DRM).nospace() << m_id << ": " << prop->name << "' (id " << prop->prop_id
                              << "): " << properties->prop_values[i];
            m_props[n] = new Property(prop.data(), properties->prop_values[i], enumNames);
            return;
        }
    }
    qCWarning(KWIN_DRM) << "Initializing property" << m_propsNames[n] << "failed";
}

bool DrmObject::atomicPopulate(drmModeAtomicReq *req) const
{
    return doAtomicPopulate(req, 0);
}

bool DrmObject::doAtomicPopulate(drmModeAtomicReq *req, int firstProperty) const
{
    bool ret = true;

    for (int i = firstProperty; i < m_props.size(); i++) {
        auto property = m_props.at(i);
        if (!property) {
            continue;
        }
        ret &= atomicAddProperty(req, property);
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic object" << m_id;
        return false;
    }
    return true;
}

void DrmObject::setValue(int prop, uint64_t new_value)
{
    Q_ASSERT(prop < m_props.size());
    auto property = m_props.at(prop);
    if (property) {
        property->setValue(new_value);
    }
}

bool DrmObject::propHasEnum(int prop, uint64_t value) const
{
    auto property = m_props.at(prop);
    return property ? property->hasEnum(value) : false;
}

bool DrmObject::atomicAddProperty(drmModeAtomicReq *req, Property *property) const
{
    if (drmModeAtomicAddProperty(req, m_id, property->propId(), property->value()) <= 0) {
        qCWarning(KWIN_DRM) << "Adding property" << property->name()
                            << "to atomic commit failed for object" << this;
        return false;
    }
    return true;
}

/*
 * Definitions for struct Prop
 */

DrmObject::Property::Property(drmModePropertyRes *prop, uint64_t val, QVector<QByteArray> enumNames)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_value(val)
{
    if (!enumNames.isEmpty()) {
        qCDebug(KWIN_DRM) << m_propName << " can have enums:" << enumNames;
        m_enumNames = enumNames;
        initEnumMap(prop);
    }
}

DrmObject::Property::~Property() = default;

void DrmObject::Property::initEnumMap(drmModePropertyRes *prop)
{
    if ( ( !(prop->flags & DRM_MODE_PROP_ENUM) && !(prop->flags & DRM_MODE_PROP_BITMASK) )
            || prop->count_enums < 1 ) {
        qCWarning(KWIN_DRM) << "Property '" << prop->name << "' ( id ="
                          << m_propId << ") should be enum valued, but it is not.";
        return;
    }

    const int nameCount = m_enumNames.size();
    m_enumMap.resize(nameCount);

    qCDebug(KWIN_DRM).nospace() << "Available are " << prop->count_enums <<
                         " enums. Query their runtime values:";

    for (int i = 0; i < prop->count_enums; i++) {
        struct drm_mode_property_enum *en = &prop->enums[i];
        int j = 0;

        while (QByteArray(en->name) != m_enumNames[j]) {
            j++;
            if (j == nameCount) {
                qCWarning(KWIN_DRM).nospace() << m_propName << " has unrecognized enum '"
                                              << en->name << "'";
                break;
            }
        }

        if (j < nameCount) {
            qCDebug(KWIN_DRM).nospace() << "Enum '" << en->name
                                        << "': runtime-value = " << en->value;
            m_enumMap[j] = en->value;
        }
    }

    if (KWIN_DRM().isDebugEnabled()) {
        for (int i = 0; i < m_enumMap.size(); i++) {
            if (m_value == m_enumMap[i]) {
                // TODO: This does not work with bitmask properties, because from kernel we get the
                //       values for some reason as the shift distance instead of the full value.
                //       See: https://github.com/torvalds/linux/blob/6794862a/drivers/
                //            gpu/drm/drm_blend.c#L267
                qCDebug(KWIN_DRM) << "=>" << m_propName
                                  << "with mapped enum value" << m_enumNames[i];
            }
        }
    }
}

}

QDebug& operator<<(QDebug& s, const KWin::DrmObject* obj)
{
    return s.nospace() << "DrmObject(" << obj->id() << ", output:" << obj->output() << ", fd: "<< obj->fd() << ')';
}
