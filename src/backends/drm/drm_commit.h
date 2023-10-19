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
#include <unordered_set>

#include "core/renderloop.h"
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
    DrmAtomicCommit(const QList<DrmPipeline *> &pipelines);
    DrmAtomicCommit(const DrmAtomicCommit &copy) = default;

    void addProperty(const DrmProperty &prop, uint64_t value);
    template<typename T>
    void addEnum(const DrmEnumProperty<T> &prop, T enumValue)
    {
        addProperty(prop, prop.valueForEnum(enumValue));
    }
    void addBlob(const DrmProperty &prop, const std::shared_ptr<DrmBlob> &blob);
    void addBuffer(DrmPlane *plane, const std::shared_ptr<DrmFramebuffer> &buffer);
    void setVrr(DrmCrtc *crtc, bool vrr);
    void setPresentationMode(PresentationMode mode);

    bool test();
    bool testAllowModeset();
    bool commit();
    bool commitModeset();

    void pageFlipped(std::chrono::nanoseconds timestamp) const override;

    bool areBuffersReadable() const;
    std::optional<bool> isVrr() const;
    const std::unordered_set<DrmPlane *> &modifiedPlanes() const;

    void merge(DrmAtomicCommit *onTop);

    void setCursorOnly(bool cursor);
    bool isCursorOnly() const;

private:
    bool doCommit(uint32_t flags);

    const QList<DrmPipeline *> m_pipelines;
    std::unordered_map<const DrmProperty *, std::shared_ptr<DrmBlob>> m_blobs;
    std::unordered_map<DrmPlane *, std::shared_ptr<DrmFramebuffer>> m_buffers;
    std::unordered_set<DrmPlane *> m_planes;
    std::optional<bool> m_vrr;
    std::unordered_map<uint32_t /* object */, std::unordered_map<uint32_t /* property */, uint64_t /* value */>> m_properties;
    bool m_cursorOnly = false;
    PresentationMode m_mode = PresentationMode::VSync;
};

class DrmLegacyCommit : public DrmCommit
{
public:
    DrmLegacyCommit(DrmPipeline *pipeline, const std::shared_ptr<DrmFramebuffer> &buffer);

    bool doModeset(DrmConnector *connector, DrmConnectorMode *mode);
    bool doPageflip(PresentationMode mode);
    void pageFlipped(std::chrono::nanoseconds timestamp) const override;

private:
    DrmPipeline *const m_pipeline;
    const std::shared_ptr<DrmFramebuffer> m_buffer;
    PresentationMode m_mode = PresentationMode::VSync;
};

}
