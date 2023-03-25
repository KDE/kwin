/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <xf86drmMode.h>

#include <QHash>

#include "drm_pointer.h"
#include "drm_property.h"

namespace KWin
{

class DrmGpu;
class DrmProperty;
class DrmBlob;

class DrmAtomicCommit
{
public:
    DrmAtomicCommit(DrmGpu *gpu);

    void addProperty(DrmProperty *prop, uint64_t value);
    void addEnum(DrmProperty *prop, auto enumValue)
    {
        addProperty(prop, prop->valueForEnum(enumValue));
    }
    void addBlob(DrmProperty *prop, const std::shared_ptr<DrmBlob> &blob);

    bool test();
    bool testAllowModeset();
    bool commit();
    bool commitModeset();

    drmModeAtomicReq *req() const;

private:
    DrmGpu *const m_gpu;
    DrmUniquePtr<drmModeAtomicReq> m_req;
    QHash<DrmProperty *, std::shared_ptr<DrmBlob>> m_blobs;
};

}
