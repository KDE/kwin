/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object.h"

#include <errno.h>

#include "drm_gpu.h"
#include "drm_pointer.h"
#include "logging.h"

namespace KWin
{

/*
 * Definitions for class DrmObject
 */

DrmObject::DrmObject(DrmGpu *gpu, uint32_t objectId, const QVector<PropertyDefinition> &&vector, uint32_t objectType)
    : m_gpu(gpu)
    , m_id(objectId)
    , m_objectType(objectType)
    , m_propertyDefinitions(vector)
{
    m_props.resize(m_propertyDefinitions.count());
}

DrmObject::~DrmObject()
{
    qDeleteAll(m_props);
}

bool DrmObject::initProps()
{
    if (!updateProperties()) {
        return false;
    }
    if (KWIN_DRM().isDebugEnabled() && m_gpu->atomicModeSetting()) {
        for (int i = 0; i < m_propertyDefinitions.count(); i++) {
            if (!m_props[i]) {
                qCDebug(KWIN_DRM) << "Could not find property" << m_propertyDefinitions[i].name;
            }
        }
    }
    return true;
}

bool DrmObject::atomicPopulate(drmModeAtomicReq *req) const
{
    for (const auto &property : qAsConst(m_props)) {
        if (property && !property->isImmutable() && !property->isLegacy() && property->needsCommit()) {
            if (drmModeAtomicAddProperty(req, m_id, property->propId(), property->pending()) <= 0) {
                qCWarning(KWIN_DRM) << "Adding property" << property->name() << "->" << property->pending()
                                    << "to atomic commit failed for object" << this << "with error" << strerror(errno);
                return false;
            }
        }
    }
    return true;
}

QVector<DrmObject::Property*> DrmObject::properties()
{
    return m_props;
}

void DrmObject::commit()
{
    for (const auto &prop : qAsConst(m_props)) {
        if (prop) {
            prop->commit();
        }
    }
}

void DrmObject::commitPending()
{
    for (const auto &prop : qAsConst(m_props)) {
        if (prop) {
            prop->commitPending();
        }
    }
}

void DrmObject::rollbackPending()
{
    for (const auto &prop : qAsConst(m_props)) {
        if (prop) {
            prop->rollbackPending();
        }
    }
}

bool DrmObject::needsCommit() const
{
    for (const auto &prop : qAsConst(m_props)) {
        if (prop && prop->needsCommit()) {
            return true;
        }
    }
    return false;
}

bool DrmObject::updateProperties()
{
    DrmScopedPointer<drmModeObjectProperties> properties(drmModeObjectGetProperties(m_gpu->fd(), m_id, m_objectType));
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for object" << m_id;
        return false;
    }
    for (int propIndex = 0; propIndex < m_propertyDefinitions.count(); propIndex++) {
        const PropertyDefinition &def = m_propertyDefinitions[propIndex];
        bool found = false;
        for (uint32_t drmPropIndex = 0; drmPropIndex < properties->count_props; drmPropIndex++) {
            DrmScopedPointer<drmModePropertyRes> prop(drmModeGetProperty(m_gpu->fd(), properties->props[drmPropIndex]));
            if (!prop) {
                qCWarning(KWIN_DRM, "Getting property %d of object %d failed!", drmPropIndex, m_id);
                continue;
            }
            if (def.name == prop->name) {
                drmModePropertyBlobRes *blob = nullptr;
                if (prop->flags & DRM_MODE_PROP_BLOB) {
                    blob = drmModeGetPropertyBlob(m_gpu->fd(), properties->prop_values[drmPropIndex]);
                }
                if (m_props[propIndex]) {
                    if (prop->flags & DRM_MODE_PROP_BLOB) {
                        m_props[propIndex]->setCurrentBlob(blob);
                    } else {
                        m_props[propIndex]->setCurrent(properties->prop_values[drmPropIndex]);
                    }
                } else {
                    qCDebug(KWIN_DRM, "Found property %s with value %lu", def.name.data(), properties->prop_values[drmPropIndex]);
                    m_props[propIndex] = new Property(m_gpu, prop.data(), properties->prop_values[drmPropIndex], def.enumNames, blob);
                }
                found = true;
                break;
            }
        }
        if (!found) {
            deleteProp(propIndex);
        }
    }
    return true;
}

/*
 * Definitions for DrmObject::Property
 */

DrmObject::Property::Property(DrmGpu *gpu, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames, drmModePropertyBlobRes *blob)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_pending(val)
    , m_pendingBlob(blob)
    , m_next(val)
    , m_nextBlob(blob)
    , m_current(val)
    , m_currentBlob(blob)
    , m_immutable(prop->flags & DRM_MODE_PROP_IMMUTABLE)
    , m_gpu(gpu)
{
    if (!enumNames.isEmpty()) {
        qCDebug(KWIN_DRM) << m_propName << " can have enums:" << enumNames;
        m_enumNames = enumNames;
        initEnumMap(prop);
    }
}

DrmObject::Property::~Property() = default;

void DrmObject::Property::setPending(uint64_t value)
{
    m_pending = value;
}

uint64_t DrmObject::Property::pending() const
{
    return m_pending;
}

bool DrmObject::Property::setPendingBlob(void *blob, size_t length)
{
    if (!blob && !m_pendingBlob) {
        return true;
    }
    if (blob && m_pendingBlob && length == m_pendingBlob->length && memcmp(blob, m_pendingBlob->data, length) == 0) {
        return true;
    }
    uint32_t id = 0;
    if (blob != nullptr) {
        if (drmModeCreatePropertyBlob(m_gpu->fd(), blob, length, &id) != 0) {
            qCWarning(KWIN_DRM) << "Creating property blob failed!" << strerror(errno);
            return false;
        }
    }
    if (m_pending && m_pending != m_current && m_pending != m_next) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_pending);
    }
    m_pending = id;
    m_pendingBlob = drmModeGetPropertyBlob(m_gpu->fd(), m_pending);
    return true;
}

drmModePropertyBlobRes *DrmObject::Property::pendingBlob() const
{
    return m_pendingBlob;
}

void DrmObject::Property::setCurrent(uint64_t value)
{
    m_current = value;
}

uint64_t DrmObject::Property::current() const
{
    return m_current;
}

void DrmObject::Property::setCurrentBlob(drmModePropertyBlobRes *blob)
{
    if (!blob && !m_currentBlob) {
        return;
    }
    uint32_t blobId = blob ? blob->id : 0;
    if (m_current && m_current != m_pending && m_current != m_next && m_current != blobId) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_current);
    }
    m_currentBlob = blob;
    m_current = blobId;
}

drmModePropertyBlobRes *DrmObject::Property::currentBlob() const
{
    return m_currentBlob;
}

void DrmObject::Property::commit()
{
    if (m_immutable || m_current == m_pending) {
        return;
    }
    if (m_pendingBlob || m_currentBlob) {
        setCurrentBlob(m_pendingBlob);
    } else {
        setCurrent(m_pending);
    }
}

void DrmObject::Property::commitPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    if (m_pendingBlob || m_nextBlob) {
        if (m_next && m_next != m_current) {
            drmModeDestroyPropertyBlob(m_gpu->fd(), m_next);
        }
        m_next = m_pending;
        m_nextBlob = m_pendingBlob;
    } else {
        m_next = m_pending;
    }
}

void DrmObject::Property::rollbackPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    if (m_pendingBlob || m_nextBlob) {
        if (m_pending && m_pending != m_current) {
            drmModeDestroyPropertyBlob(m_gpu->fd(), m_pending);
        }
        m_pending = m_next;
        m_pendingBlob = m_nextBlob;
    } else {
        m_pending = m_next;
    }
}

bool DrmObject::Property::needsCommit() const
{
    return m_pending != m_current;
}

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
            if (m_current == m_enumMap[i]) {
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
