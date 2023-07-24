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
#include <unordered_map>

#include "drm_pointer.h"
#include "drm_property.h"

namespace KWin
{

class DrmBlob;
class DrmConnector;
class DrmConnectorMode;
class DrmCrtc;
class DrmFramebuffer;
class DrmGpu;
class DrmPlane;
class DrmProperty;

class DrmCommit
{
public:
    virtual ~DrmCommit();

    DrmGpu *gpu() const;

protected:
    DrmCommit(DrmGpu *gpu);

    DrmGpu *const m_gpu;
};

class DrmAtomicCommit : public DrmCommit
{
public:
    DrmAtomicCommit(DrmGpu *gpu);
    ~DrmAtomicCommit() override;

    void addProperty(const DrmProperty &prop, uint64_t value);
    template<typename T>
    void addEnum(const DrmEnumProperty<T> &prop, T enumValue)
    {
        addProperty(prop, prop.valueForEnum(enumValue));
    }
    void addBlob(const DrmProperty &prop, const std::shared_ptr<DrmBlob> &blob);
    void addBuffer(DrmPlane *plane, const std::shared_ptr<DrmFramebuffer> &buffer);

    bool test();
    bool testAllowModeset();
    bool commit();
    bool commitModeset();

    drmModeAtomicReq *req() const;

private:
    DrmUniquePtr<drmModeAtomicReq> m_req;
    QHash<const DrmProperty *, std::shared_ptr<DrmBlob>> m_blobs;
    std::unordered_map<DrmPlane *, std::shared_ptr<DrmFramebuffer>> m_buffers;
    bool m_commitSuccessful = false;
};

class DrmLegacyCommit : public DrmCommit
{
public:
    DrmLegacyCommit(DrmCrtc *crtc, const std::shared_ptr<DrmFramebuffer> &buffer);
    ~DrmLegacyCommit() override;

    bool doModeset(DrmConnector *connector, DrmConnectorMode *mode);
    bool doPageflip(uint32_t flags);

private:
    DrmCrtc *const m_crtc;
    const std::shared_ptr<DrmFramebuffer> m_buffer;
    bool m_success = false;
};

}
