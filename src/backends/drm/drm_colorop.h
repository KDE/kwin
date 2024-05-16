#pragma once

#include "core/colorpipeline.h"
#include "drm_object.h"

#include <QObject>

namespace KWin
{

class DrmAtomicCommit;

class DrmColorOp : public DrmObject
{
    Q_GADGET
public:
    explicit DrmColorOp(DrmGpu *gpu, uint32_t pipelineId);
    ~DrmColorOp() override;

    bool updateProperties() override;
    void disable(DrmAtomicCommit *commit) override;

    enum class Type {
        Named1DLut,
        Custom1DLut,
        Matrix3x4,
        Multiplier
    };
    Q_ENUM(Type);
    enum class Curve {
        sRGB_EOTF = 0,
        BT2020_inverseEOTF = 2,
        PQ125_EOTF = 4,
    };

    DrmEnumProperty<Type> type;
    DrmProperty next;
    DrmProperty bypass;

    DrmEnumProperty<Curve> curveType;
    DrmProperty multiplier;
    DrmProperty matrixData;
    DrmProperty lutData;
    DrmProperty lutSize;

    std::unique_ptr<DrmColorOp> nextProp;

    bool matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline);

private:
    ColorPipeline m_cachedPipeline;
    std::unique_ptr<DrmAtomicCommit> m_cache;
};

}
