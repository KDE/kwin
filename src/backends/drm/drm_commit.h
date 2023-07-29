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
#include <chrono>
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
class DrmPipeline;

class DrmCommit
{
public:
    virtual ~DrmCommit();

    DrmGpu *gpu() const;
    virtual void pageFlipped(std::chrono::nanoseconds timestamp) const = 0;

protected:
    DrmCommit(DrmGpu *gpu);

    DrmGpu *const m_gpu;
};

class DrmAtomicCommit : public DrmCommit
{
public:
    DrmAtomicCommit(const QVector<DrmPipeline *> &pipelines);

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

    void pageFlipped(std::chrono::nanoseconds timestamp) const override;

    drmModeAtomicReq *req() const;

private:
    const QVector<DrmPipeline *> m_pipelines;
    DrmUniquePtr<drmModeAtomicReq> m_req;
    QHash<const DrmProperty *, std::shared_ptr<DrmBlob>> m_blobs;
    std::unordered_map<DrmPlane *, std::shared_ptr<DrmFramebuffer>> m_buffers;
};

class DrmLegacyCommit : public DrmCommit
{
public:
    DrmLegacyCommit(DrmPipeline *pipeline, const std::shared_ptr<DrmFramebuffer> &buffer);

    bool doModeset(DrmConnector *connector, DrmConnectorMode *mode);
    bool doPageflip(uint32_t flags);
    void pageFlipped(std::chrono::nanoseconds timestamp) const override;

private:
    DrmPipeline *const m_pipeline;
    const std::shared_ptr<DrmFramebuffer> m_buffer;
};

}
