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
    Q_GADGET
public:
    enum class Feature {
        Bypass = 1 << 0,
        MultipleOps = 1 << 1,
    };
    Q_DECLARE_FLAGS(Features, Feature);

    explicit DrmAbstractColorOp(DrmAbstractColorOp *next, Features features, const QString &name);
    virtual ~DrmAbstractColorOp();

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);
    virtual std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) = 0;
    virtual bool canBeInputOrOutput(ColorspaceType type) const;
    virtual void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;
    struct Scaling
    {
        ColorOp::Operation scaling;
        ColorOp inverse;
    };
    virtual std::optional<Scaling> inputScaling(const ColorOp &op) const;
    virtual std::optional<Scaling> outputScaling(const ColorOp &op) const;

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

class DrmLutColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
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

class LegacyMatrixColorOp : public DrmAbstractColorOp
{
public:
    explicit LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

class Matrix3x4ColorOp : public DrmAbstractColorOp
{
public:
    explicit Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
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

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
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

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
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
    QList<drm_color_lut> m_components;
};

class DrmMultiplier : public DrmAbstractColorOp
{
public:
    explicit DrmMultiplier(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
    void program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    std::optional<Scaling> inputScaling(const ColorOp &op) const override;
    std::optional<Scaling> outputScaling(const ColorOp &op) const override;

private:
    DrmProperty *const m_value;
    DrmProperty *const m_bypass;
};

enum class Named1DLutType {
    sRGB_EOTF,
    sRGB_Inverse_EOTF,
    PQ_125_EOTF,
    PQ_125_Inverse_EOTF,
};

class DrmNamed1DLut : public DrmAbstractColorOp
{
public:
    explicit DrmNamed1DLut(DrmAbstractColorOp *next, DrmEnumProperty<Named1DLutType> *value, DrmProperty *bypass);

    std::optional<uint32_t> colorOpPreference(const ColorOp::Operation &op) override;
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
