/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorpipeline.h"
#include "drm_object.h"

#include <QObject>
#include <drm.h>
#include <memory>
#include <span>

namespace KWin
{

// TODO use the struct and enum from libdrm header, once we depend on a version with it
struct drm_mode_3dlut_mode
{
    /**
     * @lut_size: 3D LUT size - can be 9, 17 or 33
     */
    __u16 lut_size;
    /**
     * @lut_stride: dimensions of 3D LUT. Must be larger than lut_size
     */
    __u16 lut_stride[3];
    /**
     * @interpolation: interpolation algorithm for 3D LUT. See drm_colorop_lut3d_interpolation_type
     */
    __u16 interpolation;
    /**
     * @color_depth: color depth - can be 8, 10 or 12
     */
    __u16 color_depth;
    /**
     * @color_format: color format specified by fourcc values
     * ex. DRM_FORMAT_XRGB16161616 - color in order of RGB, each is 16bit.
     */
    __u32 color_format;
    /**
     * @traversal_order:
     *
     * Traversal order when parsing/writing the 3D LUT. See enum drm_colorop_lut3d_traversal_order
     */
    __u16 traversal_order;
};
enum drm_colorop_lut3d_traversal_order {
    /**
     * @DRM_COLOROP_LUT3D_TRAVERSAL_RGB:
     *
     * the LUT elements are traversed like so:
     *   for R in range 0..n
     *     for G in range 0..n
     *       for B in range 0..n
     *         color = lut3d[R][G][B]
     */
    DRM_COLOROP_LUT3D_TRAVERSAL_RGB,
    /**
     * @DRM_COLOROP_LUT3D_TRAVERSAL_BGR:
     *
     * the LUT elements are traversed like so:
     *   for R in range 0..n
     *     for G in range 0..n
     *       for B in range 0..n
     *         color = lut3d[B][G][R]
     */
    DRM_COLOROP_LUT3D_TRAVERSAL_BGR,
};

class DrmBlob;
class DrmProperty;
class DrmAtomicCommit;

class DrmAbstractColorOp
{
    Q_GADGET
public:
    enum class Feature {
        NonLinear = 1 << 0,
        Bypass = 1 << 1,
    };
    Q_DECLARE_FLAGS(Features, Feature);

    explicit DrmAbstractColorOp(DrmAbstractColorOp *next, Features features);
    virtual ~DrmAbstractColorOp();

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);
    virtual std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) = 0;
    virtual void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;

    DrmAbstractColorOp *next() const;
    bool needsNonlinearity() const;
    bool canBypass() const;

protected:
    DrmAbstractColorOp *const m_next;
    const Features m_features;

    std::optional<ColorPipeline> m_cachedPipeline;
    std::optional<ColorPipeline> m_cachedPipelineFail;
    std::unique_ptr<DrmAtomicCommit> m_cache;
};

class DrmLutColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
    const uint32_t m_maxSize;
    QList<drm_color_lut> m_components;
};

class LegacyMatrixColorOp : public DrmAbstractColorOp
{
public:
    explicit LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class Matrix3x4ColorOp : public DrmAbstractColorOp
{
public:
    explicit Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
};

class UnknownColorOp : public DrmAbstractColorOp
{
public:
    explicit UnknownColorOp(DrmAbstractColorOp *next, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_bypass;
};

class DrmLut3DColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmLut3DColorOp(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *modeIndex, const QList<drm_mode_3dlut_mode> &modes, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_value;
    DrmProperty *const m_bypass;
    DrmProperty *const m_3dLutModeIndex;
    const QList<drm_mode_3dlut_mode> m_modes;
    QList<drm_color_lut> m_components;
};

class DrmMultiplier : public DrmAbstractColorOp
{
public:
    explicit DrmMultiplier(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_value;
    DrmProperty *const m_bypass;
};

class DrmColorOp : public DrmObject
{
public:
    explicit DrmColorOp(DrmGpu *gpu, uint32_t objectId);

    void disable(DrmAtomicCommit *commit) override;
    bool updateProperties() override;

    DrmAbstractColorOp *colorOp() const;

private:
    enum class Type : uint64_t {
        Lut1D,
        Matrix3x4,
        Lut3D,
        Multiplier,
        // TODO NamedLut for completion
    };
    DrmProperty m_next;
    DrmEnumProperty<Type> m_type;
    DrmProperty m_data;
    DrmProperty m_1dlutSize;
    DrmProperty m_bypass;
    DrmProperty m_3dLutModeIndex;
    DrmProperty m_3dLutModesBlob;
    std::unique_ptr<DrmAbstractColorOp> m_op;
    std::unique_ptr<DrmColorOp> m_nextOp;
};
}
