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
#include <deque>
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
    enum class Feature {
        Bypass = 1 << 0,
        MultipleOps = 1 << 1,
    };
    Q_DECLARE_FLAGS(Features, Feature);

    enum class Priority {
        Low = 0,
        Normal = 1,
        High = 2,
    };

    explicit DrmAbstractColorOp(DrmAbstractColorOp *next, Features features, const QString &name);
    virtual ~DrmAbstractColorOp();

    /**
     * Attempts to match the color pipeline to this chain of hardware operations
     */
    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);

    virtual std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) = 0;
    /**
     * @returns whether or not it makes sense to input or output values
     *          of the given colorspace type with this hardware operation
     */
    virtual bool canBeInputOrOutput(ColorspaceType type) const;
    virtual void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;
    struct Scaling
    {
        ColorOp::Operation scaling;
        ColorOp inverse;
    };
    /**
     * @returns the scaling that needs to be applied in order for this hardware
     *          block to be able to input values described by the ColorOp without
     *          clipping or visible banding.
     *          "scaling" needs to be applied as the first step in this hardware op,
     *          and "inverse" needs to be applied as the last step in the previous
     *          hardware op.
     *
     * @arg nextOperation is the color operation you want to apply in this
     *      hardware block. Its input colorspace type and input value range
     *      determine which scaling (if any) needs to be applied
     */
    virtual std::optional<Scaling> inputScaling(const ColorOp &nextOperation) const;
    /**
     * @returns the scaling that needs to be applied in order for this hardware
     *          block to be able to output values described by the ColorOp without
     *          clipping or visible banding.
     *          "scaling" needs to be applied as the last step in this hardware op,
     *          and "inverse" needs to be applied as the first step in the next
     *          hardware op
     *
     * @arg lastAppliedOp is the last color operation that was applied on color values,
     *      either in this hardware block or a previous one. Its output colorspace type
     *      and output value range determine which scaling (if any) needs to be applied
     */
    virtual std::optional<Scaling> outputScaling(const ColorOp &lastAppliedOp) const;

    DrmAbstractColorOp *next() const;
    bool canBypass() const;
    bool supportsMultipleOps() const;
    QString name() const;

protected:
    DrmAbstractColorOp *const m_next;
    const Features m_features;
    const QString m_name;

    std::optional<ColorPipeline> m_cachedPipeline;
    std::optional<ColorPipeline> m_cachedPipelineFail;
    std::unique_ptr<DrmAtomicCommit> m_cache;
};

enum class Lut1DInterpolation {
    Linear,
};

class DrmLutColorOp16 : public DrmAbstractColorOp
{
public:
    explicit DrmLutColorOp16(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
    DrmEnumProperty<Lut1DInterpolation> *const m_interpolationMode;
    const uint32_t m_maxSize;
    QList<drm_color_lut> m_components;
};

// TODO replace with drm_color_lut_32 once we can rely on it
struct LutComponent32
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t reserved;
};

class DrmLutColorOp32 : public DrmAbstractColorOp
{
public:
    explicit DrmLutColorOp32(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
    DrmEnumProperty<Lut1DInterpolation> *const m_interpolationMode;
    const uint32_t m_maxSize;
    QList<LutComponent32> m_components;
};

class LegacyMatrixColorOp : public DrmAbstractColorOp
{
public:
    explicit LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class Matrix3x4ColorOp : public DrmAbstractColorOp
{
public:
    explicit Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
};

class UnknownColorOp : public DrmAbstractColorOp
{
public:
    explicit UnknownColorOp(DrmAbstractColorOp *next, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_bypass;
};

enum class Lut3DInterpolation {
    Tetrahedal = 0,
};

class DrmLut3DColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmLut3DColorOp(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass, size_t size, DrmEnumProperty<Lut3DInterpolation> *interpolation);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    bool canBeInputOrOutput(ColorspaceType type) const override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    DrmProperty *const m_value;
    DrmProperty *const m_bypass;
    const size_t m_size;
    DrmEnumProperty<Lut3DInterpolation> *const m_interpolation;
    QList<LutComponent32> m_components;
};

class DrmMultiplier : public DrmAbstractColorOp
{
public:
    explicit DrmMultiplier(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    DrmProperty *const m_value;
    DrmProperty *const m_bypass;
};

enum class Named1DLutType {
    sRGB,
    sRGB_Inverse,
    PQ_125,
    PQ_125_Inverse,
    gamma22,
    gamma22_Inverse
};

class DrmNamed1DLut : public DrmAbstractColorOp
{
public:
    explicit DrmNamed1DLut(DrmAbstractColorOp *next, DrmEnumProperty<Named1DLutType> *value, DrmProperty *bypass);

    std::optional<Priority> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    std::optional<Named1DLutType> getType(const ColorOp::Operation &op) const;

    DrmEnumProperty<Named1DLutType> *const m_value;
    DrmProperty *const m_bypass;
};

class DrmColorOp : public DrmObject
{
public:
    explicit DrmColorOp(DrmGpu *gpu, uint32_t objectId);

    bool init();
    void disable(DrmAtomicCommit *commit) override;
    bool updateProperties() override;

    DrmAbstractColorOp *colorOp() const;

private:
    enum class Type : uint64_t {
        Lut1D,
        Matrix3x4,
        Lut3D,
        Multiplier,
        NamedLut1D,
    };
    DrmProperty m_next;
    DrmEnumProperty<Type> m_type;
    DrmProperty m_data;
    DrmProperty m_size;
    DrmProperty m_bypass;
    DrmProperty m_multiplier;
    DrmEnumProperty<Named1DLutType> m_1dNamedLutType;
    DrmEnumProperty<Lut1DInterpolation> m_lut1dInterpolation;
    DrmEnumProperty<Lut3DInterpolation> m_lut3dInterpolation;
    std::unique_ptr<DrmAbstractColorOp> m_op;
    std::unique_ptr<DrmColorOp> m_nextOp;
};
}
