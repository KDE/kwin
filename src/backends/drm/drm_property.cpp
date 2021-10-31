/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_property.h"
#include "drm_object.h"
#include "drm_gpu.h"
#include "logging.h"
#include <cerrno>

namespace KWin
{

DrmProperty::DrmProperty(DrmObject *obj, drmModePropertyRes *prop, uint64_t val, const QVector<QByteArray> &enumNames)
    : m_propId(prop->prop_id)
    , m_propName(prop->name)
    , m_pending(val)
    , m_next(val)
    , m_current(val)
    , m_immutable(prop->flags & DRM_MODE_PROP_IMMUTABLE)
    , m_obj(obj)
{
    if (!enumNames.isEmpty()) {
        m_enumNames = enumNames;
        initEnumMap(prop);
    }
}

DrmProperty::~DrmProperty() = default;

bool DrmProperty::setPendingBlob(void *blob, size_t length)
{
    if (!blob && !m_pendingBlob) {
        return true;
    }
    if (blob && m_pendingBlob && length == m_pendingBlob->length && memcmp(blob, m_pendingBlob->data, length) == 0) {
        return true;
    }
    uint32_t id = 0;
    if (blob != nullptr) {
        if (drmModeCreatePropertyBlob(m_obj->gpu()->fd(), blob, length, &id) != 0) {
            qCWarning(KWIN_DRM) << "Creating property blob failed!" << strerror(errno);
            return false;
        }
    }
    if (m_pendingBlob && m_pendingBlob != m_currentBlob && m_pendingBlob != m_nextBlob) {
        drmModeDestroyPropertyBlob(m_obj->gpu()->fd(), m_pendingBlob->id);
        drmModeFreePropertyBlob(m_pendingBlob);
    }
    m_pending = id;
    m_pendingBlob = drmModeGetPropertyBlob(m_obj->gpu()->fd(), m_pending);
    return true;
}

void DrmProperty::setCurrentBlob(drmModePropertyBlobRes *blob)
{
    if (m_currentBlob && m_currentBlob != m_pendingBlob && m_currentBlob != m_nextBlob && m_currentBlob != blob) {
        drmModeDestroyPropertyBlob(m_obj->gpu()->fd(), m_currentBlob->id);
        drmModeFreePropertyBlob(m_currentBlob);
    }
    m_currentBlob = blob;
    m_current = blob ? blob->id : 0;
}


void DrmProperty::commit()
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

void DrmProperty::commitPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    if (m_pendingBlob || m_nextBlob) {
        if (m_nextBlob && m_nextBlob != m_currentBlob) {
            drmModeDestroyPropertyBlob(m_obj->gpu()->fd(), m_nextBlob->id);
            drmModeFreePropertyBlob(m_nextBlob);
        }
        m_next = m_pending;
        m_nextBlob = m_pendingBlob;
    } else {
        m_next = m_pending;
    }
}

void DrmProperty::rollbackPending()
{
    if (m_immutable || m_next == m_pending) {
        return;
    }
    if (m_pendingBlob || m_nextBlob) {
        if (m_pendingBlob && m_pendingBlob != m_currentBlob) {
            drmModeDestroyPropertyBlob(m_obj->gpu()->fd(), m_pendingBlob->id);
            drmModeFreePropertyBlob(m_pendingBlob);
        }
        m_pending = m_next;
        m_pendingBlob = m_nextBlob;
    } else {
        m_pending = m_next;
    }
}

bool DrmProperty::setPropertyLegacy(uint64_t value)
{
    return drmModeObjectSetProperty(m_obj->gpu()->fd(), m_obj->id(), m_obj->type(), m_propId, value) == 0;
}

void DrmProperty::initEnumMap(drmModePropertyRes *prop)
{
    if ( ( !(prop->flags & DRM_MODE_PROP_ENUM) && !(prop->flags & DRM_MODE_PROP_BITMASK) )
            || prop->count_enums < 1 ) {
        qCWarning(KWIN_DRM) << "Property '" << prop->name << "' ( id ="
                          << m_propId << ") should be enum valued, but it is not.";
        return;
    }

    for (int i = 0; i < prop->count_enums; i++) {
        struct drm_mode_property_enum *en = &prop->enums[i];
        int j = m_enumNames.indexOf(QByteArray(en->name));
        if (j >= 0) {
            m_enumMap[j] = en->value;
        } else {
            qCWarning(KWIN_DRM, "%s has unrecognized enum '%s'", qPrintable(m_propName), en->name);
        }
    }
}

void DrmProperty::setPending(uint64_t value)
{
    m_pending = value;
}

uint64_t DrmProperty::pending() const
{
    return m_pending;
}

bool DrmProperty::needsCommit() const
{
    return m_pending != m_current;
}

drmModePropertyBlobRes *DrmProperty::currentBlob() const
{
    return m_currentBlob;
}

drmModePropertyBlobRes *DrmProperty::pendingBlob() const
{
    return m_pendingBlob;
}

void DrmProperty::setCurrent(uint64_t value)
{
    m_current = value;
}

uint64_t DrmProperty::current() const
{
    return m_current;
}

QVector<QByteArray> DrmProperty::enumNames() const
{
    return m_enumNames;
}
bool DrmProperty::hasEnum(uint64_t value) const
{
    return m_enumMap.contains(value);
}
bool DrmProperty::hasAllEnums() const
{
    return m_enumMap.count() == m_enumNames.count();
}

uint32_t DrmProperty::propId() const
{
    return m_propId;
}

const QByteArray &DrmProperty::name() const
{
    return m_propName;
}

bool DrmProperty::isImmutable() const
{
    return m_immutable;
}

bool DrmProperty::isLegacy() const
{
    return m_legacy;
}

void DrmProperty::setLegacy()
{
    m_legacy = true;
}

}
