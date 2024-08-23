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

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);
    virtual void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;
    virtual bool canBeUsedFor(const ColorOp &op, bool normalizedInput) = 0;

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

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

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

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

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

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

private:
    DrmProperty *const m_prop;
};

class Matrix3x4ColorOp : public DrmAbstractColorOp
{
public:
    explicit Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass);

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

private:
    DrmProperty *const m_prop;
    DrmProperty *const m_bypass;
};

class UnknownColorOp : public DrmAbstractColorOp
{
public:
    explicit UnknownColorOp(DrmAbstractColorOp *next, DrmProperty *bypass);

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

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

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

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

    void program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations) override;
    void bypass(DrmAtomicCommit *commit) override;
    bool canBeUsedFor(const ColorOp &op, bool normalizedInput) override;

private:
    DrmProperty *const m_value;
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
    };
    DrmProperty m_next;
    DrmEnumProperty<Type> m_type;
    DrmProperty m_data;
    DrmProperty m_size;
    DrmProperty m_bypass;
    DrmProperty m_multiplier;
    DrmEnumProperty<Lut1DInterpolation> m_lut1dInterpolation;
    DrmEnumProperty<Lut3DInterpolation> m_lut3dInterpolation;
    std::unique_ptr<DrmAbstractColorOp> m_op;
    std::unique_ptr<DrmColorOp> m_nextOp;
};
}
