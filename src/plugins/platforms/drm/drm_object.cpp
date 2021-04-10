/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object.h"
#include "drm_gpu.h"
#include "drm_pointer.h"

#include "logging.h"

namespace KWin
{

/*
 * Definitions for class DrmObject
 */

DrmObject::DrmObject(DrmGpu *gpu, uint32_t objectId)
    : m_gpu(gpu)
    , m_id(objectId)
{
}

DrmObject::~DrmObject()
{
    for (auto *p : m_props) {
        delete p;
    }
}

bool DrmObject::initProps(const QVector<PropertyDefinition> &&vector, uint32_t objectType)
{
    DrmScopedPointer<drmModeObjectProperties> properties(drmModeObjectGetProperties(m_gpu->fd(), m_id, objectType));
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for object" << m_id;
        return false;
    }
    m_props.resize(vector.count());
    for (uint32_t i = 0; i < properties->count_props; i++) {
        DrmScopedPointer<drmModePropertyRes> prop(drmModeGetProperty(m_gpu->fd(), properties->props[i]));
        if (!prop) {
            qCWarning(KWIN_DRM, "Getting property %d of object %d failed!", i, m_id);
            continue;
        }
        for (int j = 0; j < vector.count(); j++) {
            const PropertyDefinition &def = vector[j];
            if (def.name == prop->name) {
                drmModePropertyBlobRes *blob = nullptr;
                if (prop->flags & DRM_MODE_PROP_BLOB) {
                    blob = drmModeGetPropertyBlob(m_gpu->fd(), properties->prop_values[i]);
                }
                qCDebug(KWIN_DRM, "Found property %s with value %lu", def.name.data(), properties->prop_values[i]);
                m_props[j] = new Property(prop.data(), properties->prop_values[i], def.enumNames, blob);
                break;
            }
        }
    }
    if (KWIN_DRM().isDebugEnabled()) {
        for (int i = 0; i < vector.count(); i++) {
            if (!m_props[i]) {
                qCDebug(KWIN_DRM) << "Could not find property" << vector[i].name;
            }
        }
    }
    return true;
}

bool DrmObject::atomicPopulate(drmModeAtomicReq *req) const
{
    bool ret = true;

    for (const auto &property : qAsConst(m_props)) {
        if (property && !property->isImmutable() && !property->isLegacy()) {
            ret &= atomicAddProperty(req, property);
        }
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic object" << m_id;
        return false;
    }
    return true;
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

DrmObject::Property::Property(drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames, drmModePropertyBlobRes *blob)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_value(val)
    , m_immutable(prop->flags & DRM_MODE_PROP_IMMUTABLE)
    , m_blob(blob)
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

QDebug& operator<<(QDebug& s, const KWin::DrmObject *obj)
{
    return s.nospace() << "DrmObject(" << obj->id() << ", gpu: "<< obj->gpu() << ')';
}
