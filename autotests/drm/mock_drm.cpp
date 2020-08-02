/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_drm.h"

#include <QMap>
#include <QVector>

static QMap<int, QVector<_drmModeProperty>> s_drmProperties{};

namespace MockDrm
{

void addDrmModeProperties(int fd, const QVector<_drmModeProperty> &properties)
{
    s_drmProperties.insert(fd, properties);
}

}

int drmModeAtomicAddProperty(drmModeAtomicReqPtr req, uint32_t object_id, uint32_t property_id, uint64_t value)
{
    Q_UNUSED(req)
    Q_UNUSED(object_id)
    Q_UNUSED(property_id)
    Q_UNUSED(value)
    return 0;
}

drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId)
{
    auto it = s_drmProperties.find(fd);
    if (it == s_drmProperties.end()) {
        return nullptr;
    }
    auto it2 = std::find_if(it->constBegin(), it->constEnd(),
        [propertyId] (const auto &property) {
            return property.prop_id == propertyId;
        }
    );
    if (it2 == it->constEnd()) {
        return nullptr;
    }

    auto *property = new _drmModeProperty;
    property->prop_id = it2->prop_id;
    property->flags = it2->flags;
    strcpy(property->name, it2->name);
    property->count_values = it2->count_values;
    property->values = it2->values;
    property->count_enums = it2->count_enums;
    property->enums = it2->enums;
    property->count_blobs = it2->count_blobs;
    property->blob_ids = it2->blob_ids;

    return property;
}

void drmModeFreeProperty(drmModePropertyPtr ptr)
{
    delete ptr;
}
