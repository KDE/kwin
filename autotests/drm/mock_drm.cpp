/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
