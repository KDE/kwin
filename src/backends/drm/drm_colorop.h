/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorpipeline.h"
#include "drm_plane.h"

#include <drm.h>
#include <memory>
#include <span>

namespace KWin
{

class DrmBlob;
class DrmProperty;
class DrmAtomicCommit;

class DrmAbstractColorOp
{
public:
    explicit DrmAbstractColorOp(DrmAbstractColorOp *next, bool normalized);
    virtual ~DrmAbstractColorOp();

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);
    virtual bool canBeUsedFor(const ColorOp &op) = 0;
    struct Scaling
    {
        double offset = 0;
        double scale = 1;
    };
    virtual void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;

    DrmAbstractColorOp *next() const;
    /**
     * @returns if this operation is limited to a range of [0; 1] for input or output values
     */
    bool isNormalized() const;

protected:
    DrmAbstractColorOp *m_next = nullptr;
    const bool m_normalized;

    std::optional<ColorPipeline> m_cachedPipeline;
    std::unique_ptr<DrmAtomicCommit> m_cache;
};

class LegacyLutColorOp : public DrmAbstractColorOp
{
public:
    explicit LegacyLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> opsop, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
    const uint32_t m_maxSize;
    QList<drm_color_lut> m_components;
};

class LegacyMatrixColorOp : public DrmAbstractColorOp
{
public:
    explicit LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class DrmMatrixColorOp3x4 : public DrmAbstractColorOp
{
public:
    explicit DrmMatrixColorOp3x4(DrmAbstractColorOp *next, DrmProperty *prop);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class DrmMultiplierColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmMultiplierColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class DrmDivisorColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmDivisorColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class NvPlaneGammaTf : public DrmAbstractColorOp
{
public:
    explicit NvPlaneGammaTf(DrmAbstractColorOp *next, DrmEnumProperty<NvDrmTransferFunction> *prop, bool degamma);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmEnumProperty<NvDrmTransferFunction> *const m_prop;
    const bool m_degamma;
};

/**
 * Special LUT that only applies to the first component
 */
class NvTMOLUT : public DrmAbstractColorOp
{
public:
    explicit NvTMOLUT(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize);

    bool canBeUsedFor(const ColorOp &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> ops, Scaling inputScale, Scaling outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
    const uint32_t m_maxSize;
    QList<drm_color_lut> m_components;
};
}
