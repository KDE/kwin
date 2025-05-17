/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorpipeline.h"

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

    explicit DrmAbstractColorOp(DrmAbstractColorOp *next, Features features, const QString &name);
    virtual ~DrmAbstractColorOp();

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);
    virtual bool canBeUsedFor(const ColorOp &op, bool scaling) = 0;
    virtual void program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale) = 0;
    virtual void bypass(DrmAtomicCommit *commit) = 0;

    DrmAbstractColorOp *next() const;
    bool canBypass() const;
    bool supportsMultipleOps() const;
    QString name() const;

protected:
    DrmAbstractColorOp *const m_next;
    const Features m_features;
    const QString m_name;

    std::optional<ColorPipeline> m_cachedPipeline;
    std::unique_ptr<DrmAtomicCommit> m_cache;
};

class DrmLutColorOp : public DrmAbstractColorOp
{
public:
    explicit DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize);

    bool canBeUsedFor(const ColorOp &op, bool scaling) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale) override;
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

    bool canBeUsedFor(const ColorOp &op, bool scaling) override;
    void program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale) override;
    void bypass(DrmAtomicCommit *commit) override;

private:
    DrmProperty *const m_prop;
};

}
