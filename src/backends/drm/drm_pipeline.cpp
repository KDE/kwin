/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "core/iccprofile.h"
#include "core/session.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_commit_thread.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_plane.h"
#include "utils/drm_format_helper.h"
#include "utils/envvar.h"
#include "utils/kernel.h"

#include <drm_fourcc.h>
#include <gbm.h>

using namespace std::literals;

namespace KWin
{

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
    , m_commitThread(std::make_unique<DrmCommitThread>(conn->gpu(), conn->connectorName()))
{
}

DrmPipeline::~DrmPipeline()
{
    // the commit thread may still access the pipeline until it's stopped
    // so it must be deleted before everything else
    m_commitThread.reset();
}

DrmPipeline::Error DrmPipeline::testPresent(const std::shared_ptr<OutputFrame> &frame)
{
    if (!gpu()->atomicModeSetting()) {
        // we can do nothing but hope for the best
        // the compositor will have to do a fallback for when the real present fails
        return Error::None;
    }
    // test the full state with all planes, to take pending commits into account
    return DrmPipeline::commitPipelinesAtomic({this}, CommitMode::Test, frame, {});
}

DrmPipeline::Error DrmPipeline::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    Q_ASSERT(m_pending.crtc);
    if (gpu()->atomicModeSetting()) {
        // NOTE that this assumes testPresentation has been called before and succeeded
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto partialUpdate = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        if (Error err = prepareAtomicPresentation(partialUpdate.get(), frame); err != Error::None) {
            return err;
        }
        for (const auto layer : layersToUpdate) {
            const auto pipelineLayer = static_cast<DrmPipelineLayer *>(layer);
            if (Error err = prepareAtomicPlane(partialUpdate.get(), pipelineLayer->plane(), pipelineLayer, frame); err != Error::None) {
                return err;
            }
        }
        if (layersToUpdate.isEmpty()) {
            // work around amdgpu not giving us a valid pageflip timestamp
            // if the commit doesn't contain a drm plane
            if (Error err = prepareAtomicPlane(partialUpdate.get(), m_pending.layers.front()->plane(), m_pending.layers.front(), frame); err != Error::None) {
                return err;
            }
        }
        if (m_pending.needsModesetProperties && !prepareAtomicModeset(partialUpdate.get())) {
            return Error::InvalidArguments;
        }
        m_next.needsModesetProperties = m_pending.needsModesetProperties = false;
        m_commitThread->addCommit(std::move(partialUpdate));
        return Error::None;
    } else {
        return presentLegacy(layersToUpdate, frame);
    }
}

void DrmPipeline::maybeModeset(const std::shared_ptr<OutputFrame> &frame)
{
    m_modesetPresentPending = true;
    gpu()->maybeModeset(this, frame);
}

DrmPipeline::Error DrmPipeline::commitPipelines(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, nullptr, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode, unusedObjects);
    }
}

DrmPipeline::Error DrmPipeline::commitPipelinesAtomic(const QList<DrmPipeline *> &pipelines, CommitMode mode, const std::shared_ptr<OutputFrame> &frame, const QList<DrmObject *> &unusedObjects)
{
    auto commit = std::make_unique<DrmAtomicCommit>(pipelines);
    if (mode == CommitMode::Test) {
        // if there's a modeset pending, the tests on top of that state
        // also have to allow modesets or they'll always fail
        const bool wantsModeset = std::ranges::any_of(pipelines, [](DrmPipeline *pipeline) {
            return pipeline->needsModeset();
        });
        if (wantsModeset) {
            mode = CommitMode::TestAllowModeset;
        }
    }
    for (DrmPipeline *pipeline : pipelines) {
        if (Error err = pipeline->prepareAtomicCommit(commit.get(), mode, frame); err != Error::None) {
            return err;
        }
    }
    for (DrmObject *unused : unusedObjects) {
        unused->disable(commit.get());
    }
    switch (mode) {
    case CommitMode::TestAllowModeset: {
        if (!commit->testAllowModeset()) {
            qCWarning(KWIN_DRM) << "Atomic modeset test failed!" << strerror(errno);
            return errnoToError();
        }
        const bool withoutModeset = std::ranges::all_of(pipelines, [&frame](DrmPipeline *pipeline) {
            // always require a modeset for turning off displays, it makes other logic easier to follow
            const bool oldActive = pipeline->m_next.enabled && pipeline->m_next.active;
            if (oldActive && !pipeline->activePending()) {
                return false;
            }
            auto commit = std::make_unique<DrmAtomicCommit>(QVector<DrmPipeline *>{pipeline});
            return pipeline->prepareAtomicCommit(commit.get(), CommitMode::TestAllowModeset, frame) == Error::None && commit->test();
        });
        for (DrmPipeline *pipeline : pipelines) {
            pipeline->m_pending.needsModeset = !withoutModeset;
            pipeline->m_pending.needsModesetProperties = true;
        }
        return Error::None;
    }
    case CommitMode::CommitModeset: {
        // The kernel fails commits with DRM_MODE_PAGE_FLIP_EVENT when a crtc is disabled in the commit
        // and already was disabled before, to work around some quirks in old userspace.
        // Instead of using DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, do the modeset in a blocking
        // fashion without page flip events and trigger the pageflip notification directly
        if (!commit->commitModeset()) {
            qCCritical(KWIN_DRM) << "Atomic modeset commit failed!" << strerror(errno);
            return errnoToError();
        }
        for (const auto pipeline : pipelines) {
            pipeline->m_next.needsModeset = pipeline->m_pending.needsModeset = false;
        }
        commit->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
        return Error::None;
    }
    case CommitMode::Test: {
        if (!commit->test()) {
            return errnoToError();
        }
        return Error::None;
    }
    default:
        Q_UNREACHABLE();
    }
}

DrmPipeline::Error DrmPipeline::prepareAtomicCommit(DrmAtomicCommit *commit, CommitMode mode, const std::shared_ptr<OutputFrame> &frame)
{
    if (activePending()) {
        if (Error err = prepareAtomicPresentation(commit, frame); err != Error::None) {
            return err;
        }
        for (const auto &layer : m_pending.layers) {
            if (Error err = prepareAtomicPlane(commit, layer->plane(), layer, frame); err != Error::None) {
                return err;
            }
        }
        if (mode == CommitMode::TestAllowModeset || mode == CommitMode::CommitModeset || m_pending.needsModesetProperties) {
            if (!prepareAtomicModeset(commit)) {
                return Error::InvalidArguments;
            }
        }
    } else {
        prepareAtomicDisable(commit);
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::prepareAtomicPresentation(DrmAtomicCommit *commit, const std::shared_ptr<OutputFrame> &frame)
{
    commit->setPresentationMode(m_pending.presentationMode);
    if (m_connector->contentType.isValid()) {
        commit->addEnum(m_connector->contentType, m_pending.contentType);
    }

    if (m_pending.crtc->vrrEnabled.isValid()) {
        commit->setVrr(m_pending.crtc, m_pending.presentationMode == PresentationMode::AdaptiveSync || m_pending.presentationMode == PresentationMode::AdaptiveAsync);
    }

    const ColorPipeline &colorPipeline = m_pending.crtcColorPipeline;
    if (!m_pending.crtc->postBlendingPipeline) {
        if (!colorPipeline.isIdentity()) {
            return Error::InvalidArguments;
        }
    } else {
        if (!m_pending.crtc->postBlendingPipeline->matchPipeline(commit, colorPipeline)) {
            return Error::InvalidArguments;
        }
    }

    return Error::None;
}

DrmPipeline::Error DrmPipeline::prepareAtomicPlane(DrmAtomicCommit *commit, DrmPlane *plane, DrmPipelineLayer *layer, const std::shared_ptr<OutputFrame> &frame)
{
    if (!layer->isEnabled()) {
        plane->disable(commit);
        return Error::None;
    }
    if (!layer->currentBuffer()) {
        qCWarning(KWIN_DRM) << "An enabled plane has no buffer!";
        return Error::TestBufferFailed;
    }
    const auto fb = layer->currentBuffer();
    if (!fb) {
        return Error::InvalidArguments;
    }
    const auto transform = layer->offloadTransform();
    const auto planeTransform = DrmPlane::outputTransformToPlaneTransform(transform);
    if (plane->rotation.isValid()) {
        if (!plane->rotation.hasEnum(planeTransform)) {
            return Error::InvalidArguments;
        }
        commit->addEnum(plane->rotation, planeTransform);
    } else if (planeTransform != DrmPlane::Transformation::Rotate0) {
        return Error::InvalidArguments;
    }
    commit->addProperty(plane->crtcId, m_pending.crtc->id());
    commit->addBuffer(plane, fb, frame);
    plane->set(commit, layer->sourceRect().toRect(), layer->targetRect());
    if (plane->vmHotspotX.isValid() && plane->vmHotspotY.isValid()) {
        commit->addProperty(plane->vmHotspotX, std::round(layer->hotspot().x()));
        commit->addProperty(plane->vmHotspotY, std::round(layer->hotspot().y()));
    }

    if (plane->alpha.isValid()) {
        commit->addProperty(plane->alpha, plane->alpha.maxValue());
    }
    if (plane->pixelBlendMode.isValid()) {
        commit->addEnum(plane->pixelBlendMode, DrmPlane::PixelBlendMode::PreMultiplied);
    }
    if (plane->zpos.isValid() && !plane->zpos.isImmutable()) {
        commit->addProperty(plane->zpos, layer->zpos());
    }

    const auto colorPipelines = plane->colorPipelines();
    if (layer->colorPipeline().isIdentity()) {
        if (plane->colorPipeline.isValid()) {
            commit->addProperty(plane->colorPipeline, 0);
        }
    } else {
        const auto it = std::ranges::find_if(colorPipelines, [&](DrmColorOp *pipeline) {
            return pipeline->colorOp()->matchPipeline(commit, layer->colorPipeline());
        });
        if (it == colorPipelines.end()) {
            // TODO re-allow merging with post-blending pipeline
            return DrmPipeline::Error::InvalidArguments;
        }
        commit->addProperty(plane->colorPipeline, (*it)->id());
    }

    if (plane->colorPipeline.isValid() && layer->colorDescription()->yuvCoefficients() != YUVMatrixCoefficients::Identity) {
        // color pipelines don't support the color encoding and color range properties yet
        return Error::InvalidArguments;
    }
    DrmPlane::ColorRange range = DrmPlane::ColorRange::Limited_YCbCr;
    if (layer->colorDescription()->range() == EncodingRange::Full) {
        range = DrmPlane::ColorRange::Full_YCbCr;
    }
    switch (layer->colorDescription()->yuvCoefficients()) {
    case YUVMatrixCoefficients::Identity:
        if (layer->colorDescription()->range() == EncodingRange::Limited) {
            return Error::InvalidArguments;
        }
        break;
    case YUVMatrixCoefficients::BT601:
        if (!plane->colorEncoding.isValid() || !plane->colorRange.isValid()) {
            return Error::InvalidArguments;
        }
        commit->addEnum(plane->colorEncoding, DrmPlane::ColorEncoding::BT601_YCbCr);
        commit->addEnum(plane->colorRange, range);
        break;
    case YUVMatrixCoefficients::BT709:
        if (!plane->colorEncoding.isValid() || !plane->colorRange.isValid()) {
            return Error::InvalidArguments;
        }
        commit->addEnum(plane->colorEncoding, DrmPlane::ColorEncoding::BT709_YCbCr);
        commit->addEnum(plane->colorRange, range);
        break;
    case YUVMatrixCoefficients::BT2020:
        if (!plane->colorEncoding.isValid() || !plane->colorRange.isValid()) {
            return Error::InvalidArguments;
        }
        commit->addEnum(plane->colorEncoding, DrmPlane::ColorEncoding::BT2020_YCbCr);
        commit->addEnum(plane->colorRange, range);
        break;
    }
    return Error::None;
}

void DrmPipeline::prepareAtomicDisable(DrmAtomicCommit *commit)
{
    m_connector->disable(commit);
    if (m_pending.crtc) {
        m_pending.crtc->disable(commit);
        for (const auto layer : m_pending.layers) {
            if (DrmPlane *plane = layer->plane()) {
                plane->disable(commit);
            }
        }
    }
}

static const auto s_forceScalingMode = []() -> std::optional<DrmConnector::ScalingMode> {
    const auto env = qEnvironmentVariable("KWIN_DRM_FORCE_SCALING_MODE");
    if (env == "NONE") {
        return DrmConnector::ScalingMode::None;
    } else if (env == "FULL") {
        return DrmConnector::ScalingMode::Full;
    } else if (env == "CENTER") {
        return DrmConnector::ScalingMode::Center;
    } else if (env == "FULL_ASPECT") {
        return DrmConnector::ScalingMode::Full_Aspect;
    } else {
        return std::nullopt;
    }
}();

bool DrmPipeline::prepareAtomicModeset(DrmAtomicCommit *commit)
{
    commit->addProperty(m_connector->crtcId, m_pending.crtc->id());
    if (m_connector->broadcastRGB.isValid()) {
        commit->addEnum(m_connector->broadcastRGB, DrmConnector::rgbRangeToBroadcastRgb(m_pending.rgbRange));
    }
    if (m_connector->linkStatus.isValid()) {
        commit->addEnum(m_connector->linkStatus, DrmConnector::LinkStatus::Good);
    }
    if (m_connector->overscan.isValid()) {
        commit->addProperty(m_connector->overscan, m_pending.overscan);
    } else if (m_connector->underscan.isValid()) {
        const uint32_t hborder = calculateUnderscan();
        commit->addEnum(m_connector->underscan, m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
        commit->addProperty(m_connector->underscanVBorder, m_pending.overscan);
        commit->addProperty(m_connector->underscanHBorder, hborder);
    }
    if (m_connector->maxBpc.isValid()) {
        commit->addProperty(m_connector->maxBpc, std::clamp<uint32_t>(m_pending.maxBpc, m_connector->maxBpc.minValue(), m_connector->maxBpc.maxValue()));
    }
    if (m_connector->hdrMetadata.isValid()) {
        commit->addBlob(m_connector->hdrMetadata, createHdrMetadata(m_pending.hdr ? TransferFunction::PerceptualQuantizer : TransferFunction::gamma22));
    } else if (m_pending.hdr) {
        return false;
    }
    if (m_pending.wcg) {
        if (!m_connector->colorspace.isValid() || !m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_RGB)) {
            return false;
        }
        commit->addEnum(m_connector->colorspace, DrmConnector::Colorspace::BT2020_RGB);
    } else if (m_connector->colorspace.isValid()) {
        commit->addEnum(m_connector->colorspace, DrmConnector::Colorspace::Default);
    }
    if (m_connector->scalingMode.isValid()) {
        if (s_forceScalingMode.has_value()) {
            if (m_connector->scalingMode.hasEnum(*s_forceScalingMode)) {
                commit->addEnum(m_connector->scalingMode, *s_forceScalingMode);
            } else if (m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
                commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::None);
            }
        } else if (m_connector->isInternal()
                   && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::Full_Aspect)
                   && (m_pending.mode->flags() & OutputMode::Flag::Generated)
                   && !(m_pending.mode->flags() & OutputMode::Flag::Custom)) {
            commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::Full_Aspect);
        } else if (m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
            commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::None);
        }
    }

    commit->addProperty(m_pending.crtc->active, 1);
    commit->addBlob(m_pending.crtc->modeId, m_pending.mode->blob());
    if (m_pending.crtc->degammaLut.isValid()) {
        commit->addProperty(m_pending.crtc->degammaLut, 0);
    }

    if (m_pending.crtc->sharpnessStrength.isValid()) {
        const int maxValue = m_pending.crtc->sharpnessStrength.maxValue();
        const int sharpness = std::clamp<int>(std::round(m_output->sharpnessSetting() * maxValue), 0, maxValue);
        commit->addProperty(m_pending.crtc->sharpnessStrength, sharpness);
    }

    return true;
}

uint32_t DrmPipeline::calculateUnderscan()
{
    const auto size = m_pending.mode->size();
    const float aspectRatio = size.width() / static_cast<float>(size.height());
    uint32_t hborder = m_pending.overscan * aspectRatio;
    if (hborder > 128) {
        // overscan only goes from 0-100 so we cut off the 101-128 value range of underscan_vborder
        hborder = 128;
        m_pending.overscan = 128 / aspectRatio;
    }
    return hborder;
}

DrmPipeline::Error DrmPipeline::errnoToError()
{
    switch (errno) {
    case EINVAL:
        return Error::InvalidArguments;
    case EBUSY:
        return Error::FramePending;
    case ENOMEM:
        return Error::OutofMemory;
    case EACCES:
        return Error::NoPermission;
    default:
        return Error::Unknown;
    }
}

bool DrmPipeline::presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    if (needsModeset() || !m_pending.crtc || !m_pending.active) {
        return false;
    }
    // We need to make sure that on vmwgfx software cursor is selected
    // until Broadcom fixes hw cursor issues with vmwgfx. Otherwise
    // the cursor is missing.
    if (gpu()->isVmwgfx()) {
        return false;
    }
    const auto drmLayer = static_cast<DrmPipelineLayer *>(layer);
    if (drmLayer->plane()) {
        // test the full state, to take pending commits into account
        if (DrmPipeline::commitPipelinesAtomic({this}, CommitMode::Test, nullptr, {}) != Error::None) {
            return false;
        }
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto partialUpdate = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        prepareAtomicPlane(partialUpdate.get(), drmLayer->plane(), drmLayer, nullptr);
        partialUpdate->setAllowedVrrDelay(allowedVrrDelay);
        m_commitThread->addCommit(std::move(partialUpdate));
        return true;
    } else {
        return setCursorLegacy(drmLayer);
    }
}

void DrmPipeline::applyPendingChanges()
{
    const bool layersChanged = m_next.layers != m_pending.layers;
    m_next = m_pending;
    m_commitThread->setModeInfo(m_pending.mode->refreshRate(), m_pending.mode->vblankTime());
    m_output->renderLoop()->setPresentationSafetyMargin(m_commitThread->safetyMargin());
    m_output->renderLoop()->setRefreshRate(m_pending.mode->refreshRate());
    if (layersChanged) {
        Q_EMIT m_output->outputLayersChanged();
    }
}

DrmConnector *DrmPipeline::connector() const
{
    return m_connector;
}

DrmGpu *DrmPipeline::gpu() const
{
    return m_connector->gpu();
}

void DrmPipeline::pageFlipped(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate::get(m_output->renderLoop())->notifyVblank(timestamp);
    m_commitThread->pageFlipped(timestamp);
    // the commit thread adjusts the safety margin on every commit
    m_output->renderLoop()->setPresentationSafetyMargin(m_commitThread->safetyMargin());
    if (gpu()->needsModeset()) {
        gpu()->maybeModeset(nullptr, nullptr);
    }
}

void DrmPipeline::setOutput(DrmOutput *output)
{
    m_output = output;
    for (const auto layer : m_pending.layers) {
        layer->setOutput(output);
    }
}

DrmOutput *DrmPipeline::output() const
{
    return m_output;
}

bool DrmPipeline::needsModeset() const
{
    return m_pending.needsModeset;
}

bool DrmPipeline::activePending() const
{
    return m_pending.crtc && m_pending.mode && m_pending.active;
}

void DrmPipeline::revertPendingChanges()
{
    m_pending = m_next;
}

DrmCommitThread *DrmPipeline::commitThread() const
{
    return m_commitThread.get();
}

bool DrmPipeline::modesetPresentPending() const
{
    return m_modesetPresentPending;
}

void DrmPipeline::resetModesetPresentPending()
{
    m_modesetPresentPending = false;
}

DrmCrtc *DrmPipeline::crtc() const
{
    return m_pending.crtc;
}

std::shared_ptr<DrmConnectorMode> DrmPipeline::mode() const
{
    return m_pending.mode;
}

bool DrmPipeline::active() const
{
    return m_pending.active;
}

bool DrmPipeline::enabled() const
{
    return m_pending.enabled;
}

QList<DrmPipelineLayer *> DrmPipeline::layers() const
{
    return m_pending.layers;
}

PresentationMode DrmPipeline::presentationMode() const
{
    return m_pending.presentationMode;
}

uint32_t DrmPipeline::overscan() const
{
    return m_pending.overscan;
}

BackendOutput::RgbRange DrmPipeline::rgbRange() const
{
    return m_pending.rgbRange;
}

DrmConnector::DrmContentType DrmPipeline::contentType() const
{
    return m_pending.contentType;
}

const std::shared_ptr<IccProfile> &DrmPipeline::iccProfile() const
{
    return m_pending.iccProfile;
}

void DrmPipeline::setCrtc(DrmCrtc *crtc)
{
    m_pending.crtc = crtc;
}

void DrmPipeline::setMode(const std::shared_ptr<DrmConnectorMode> &mode)
{
    m_pending.mode = mode;
}

void DrmPipeline::setActive(bool active)
{
    m_pending.active = active;
}

void DrmPipeline::setEnable(bool enable)
{
    m_pending.enabled = enable;
}

void DrmPipeline::setLayers(const QList<DrmPipelineLayer *> &layers)
{
    m_pending.layers = layers;
    for (const auto layer : layers) {
        layer->setOutput(m_output);
    }
}

void DrmPipeline::setPresentationMode(PresentationMode mode)
{
    m_pending.presentationMode = mode;
}

void DrmPipeline::setOverscan(uint32_t overscan)
{
    m_pending.overscan = overscan;
}

void DrmPipeline::setRgbRange(BackendOutput::RgbRange range)
{
    m_pending.rgbRange = range;
}

void DrmPipeline::setCrtcColorPipeline(const ColorPipeline &pipeline)
{
    m_pending.crtcColorPipeline = pipeline;
}

void DrmPipeline::setHighDynamicRange(bool hdr)
{
    m_pending.hdr = hdr;
}

void DrmPipeline::setWideColorGamut(bool wcg)
{
    m_pending.wcg = wcg;
}

void DrmPipeline::setMaxBpc(uint32_t max)
{
    m_pending.maxBpc = max;
}

void DrmPipeline::setContentType(DrmConnector::DrmContentType type)
{
    m_pending.contentType = type;
}

void DrmPipeline::setIccProfile(const std::shared_ptr<IccProfile> &profile)
{
    m_pending.iccProfile = profile;
}

std::shared_ptr<DrmBlob> DrmPipeline::createHdrMetadata(TransferFunction::Type transferFunction) const
{
    if (transferFunction != TransferFunction::PerceptualQuantizer) {
        // for sRGB / gamma 2.2, don't send any metadata, to ensure the non-HDR experience stays the same
        return nullptr;
    }
    if (!m_connector->edid()->supportsPQ()) {
        return nullptr;
    }
    const auto colorimetry = m_connector->edid()->nativeColorimetry().value_or(Colorimetry::BT709);
    const xyY red = colorimetry.red().toxyY();
    const xyY green = colorimetry.green().toxyY();
    const xyY blue = colorimetry.blue().toxyY();
    const xyY white = colorimetry.white().toxyY();
    const auto to16Bit = [](float value) {
        return uint16_t(std::round(value / 0.00002));
    };
    hdr_output_metadata data{
        .metadata_type = 0,
        .hdmi_metadata_type1 = hdr_metadata_infoframe{
            // eotf types (from CTA-861-G page 85):
            // - 0: traditional gamma, SDR
            // - 1: traditional gamma, HDR
            // - 2: SMPTE ST2084
            // - 3: hybrid Log-Gamma based on BT.2100-0
            // - 4-7: reserved
            .eotf = uint8_t(2),
            // there's only one type. 1-7 are reserved for future use
            .metadata_type = 0,
            // in 0.00002 nits
            .display_primaries = {
                {to16Bit(red.x), to16Bit(red.y)},
                {to16Bit(green.x), to16Bit(green.y)},
                {to16Bit(blue.x), to16Bit(blue.y)},
            },
            .white_point = {to16Bit(white.x), to16Bit(white.y)},
            // in nits
            .max_display_mastering_luminance = uint16_t(std::round(m_connector->edid()->desiredMaxFrameAverageLuminance().value_or(0))),
            // in 0.0001 nits
            .min_display_mastering_luminance = uint16_t(std::round(m_connector->edid()->desiredMinLuminance() * 10000)),
            // in nits
            .max_cll = uint16_t(std::round(m_connector->edid()->desiredMaxFrameAverageLuminance().value_or(0))),
            .max_fall = uint16_t(std::round(m_connector->edid()->desiredMaxFrameAverageLuminance().value_or(0))),
        },
    };
    return DrmBlob::create(gpu(), &data, sizeof(data));
}

std::chrono::nanoseconds DrmPipeline::presentationDeadline() const
{
    return m_commitThread->safetyMargin();
}
}
