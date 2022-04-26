
#include "colortransformation.h"

#include <lcms2.h>

#include "colorpipelinestage.h"
#include "utils/common.h"

namespace KWin
{

ColorTransformation::ColorTransformation(const QVector<QSharedPointer<ColorPipelineStage>> &stages)
    : m_pipeline(cmsPipelineAlloc(nullptr, 3, 3))
{
    if (!m_pipeline) {
        qCWarning(KWIN_CORE) << "Failed to allocate cmsPipeline!";
        m_valid = false;
        return;
    }
    for (const auto &stage : stages) {
        if (stage) {
            const auto dup = stage->dup();
            if (!dup) {
                m_valid = false;
                return;
            }
            m_stages << dup;
            if (!cmsPipelineInsertStage(m_pipeline, cmsAT_END, dup->stage())) {
                qCWarning(KWIN_CORE) << "Failed to insert cmsPipeline stage!";
                m_valid = false;
                return;
            }
        }
    }
}

ColorTransformation::~ColorTransformation()
{
    if (m_pipeline) {
        cmsStage *last = nullptr;
        do {
            cmsPipelineUnlinkStage(m_pipeline, cmsAT_END, &last);
        } while (last);
        cmsPipelineFree(m_pipeline);
    }
}

bool ColorTransformation::valid() const
{
    return m_valid;
}

std::tuple<uint16_t, uint16_t, uint16_t> ColorTransformation::transform(uint16_t r, uint16_t g, uint16_t b) const
{
    const uint16_t in[3] = {r, g, b};
    uint16_t out[3] = {0, 0, 0};
    cmsPipelineEval16(in, out, m_pipeline);
    return {out[0], out[1], out[2]};
}

}
