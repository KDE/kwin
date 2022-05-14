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

class DrmObject
{
public:
    virtual ~DrmObject() = default;
    DrmObject(const DrmObject &) = delete;

    /**
     * Must be called to query necessary data directly after creation.
     * @return true when initializing was successful
     */
    virtual bool init() = 0;

    /**
     * Set the properties in such a way that this resource won't be used anymore
     */
    virtual void disable() = 0;

    uint32_t id() const;
    DrmGpu *gpu() const;
    uint32_t type() const;
    QString typeName() const;

    void commit();
    void commitPending();
    void rollbackPending();
    bool atomicPopulate(drmModeAtomicReq *req) const;
    bool needsCommit() const;
    virtual bool updateProperties();

    template<typename T>
    bool setPending(T prop, uint64_t new_value)
    {
        if (auto &property = m_props.at(static_cast<uint32_t>(prop))) {
            property->setPending(new_value);
            return true;
        }
        return false;
    }

    template<typename T>
    DrmProperty *getProp(T propIndex) const
    {
        return m_props[static_cast<uint32_t>(propIndex)].get();
    }

    enum class PrintMode {
        OnlyChanged,
        All
    };
    void printProps(PrintMode mode);

protected:
    enum class Requirement {
        Required,
        RequiredForLegacy,
        Optional,
    };
    struct PropertyDefinition
    {
        PropertyDefinition(const QByteArray &name, Requirement requirement, const QVector<QByteArray> &&enumNames = {})
            : name(name)
            , requirement(requirement)
            , enumNames(enumNames)
        {
        }
        QByteArray name;
        Requirement requirement;
        QVector<QByteArray> enumNames;
    };

    DrmObject(DrmGpu *gpu, uint32_t objectId, const QVector<PropertyDefinition> &&vector, uint32_t objectType);

    bool initProps();

    std::vector<std::unique_ptr<DrmProperty>> m_props;

private:
    DrmGpu *m_gpu;
    const uint32_t m_id;
    const uint32_t m_objectType;
    const QVector<PropertyDefinition> m_propertyDefinitions;
};

}

QDebug operator<<(QDebug stream, const KWin::DrmObject *);
