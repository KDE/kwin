/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QMap>
#include <QVector>

#include <vector>

// drm
#include <xf86drmMode.h>

#include "drm_pointer.h"
#include "drm_property.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class DrmOutput;
class DrmAtomicCommit;

class DrmPropertyList
{
public:
    void addProperty(DrmUniquePtr<drmModePropertyRes> &&prop, uint64_t value);
    std::optional<std::pair<DrmUniquePtr<drmModePropertyRes>, uint64_t>> takeProperty(const QByteArray &name);

private:
    std::vector<std::pair<DrmUniquePtr<drmModePropertyRes>, uint64_t>> m_properties;
};

class DrmObject
{
public:
    virtual ~DrmObject() = default;
    DrmObject(const DrmObject &) = delete;

    /**
     * Must be called to query necessary data directly after creation.
     * @return true when initializing was successful
     */
    bool init();

    /**
     * Set the properties in such a way that this resource won't be used anymore
     */
    virtual void disable(DrmAtomicCommit *commit) = 0;

    virtual bool updateProperties() = 0;

    uint32_t id() const;
    DrmGpu *gpu() const;
    uint32_t type() const;
    QString typeName() const;

protected:
    DrmObject(DrmGpu *gpu, uint32_t objectId, uint32_t objectType);

    DrmPropertyList queryProperties() const;

private:
    DrmGpu *m_gpu;
    const uint32_t m_id;
    const uint32_t m_objectType;
};

}

QDebug operator<<(QDebug stream, const KWin::DrmObject *);
