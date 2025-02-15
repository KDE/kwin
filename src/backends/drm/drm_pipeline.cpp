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

#include <drm_fourcc.h>
#include <gbm.h>

using namespace std::literals;

namespace KWin
{

static const QList<uint64_t> implicitModifier = {DRM_FORMAT_MOD_INVALID};
static const QHash<uint32_t, QList<uint64_t>> legacyFormats = {{DRM_FORMAT_XRGB8888, implicitModifier}};
static const QHash<uint32_t, QList<uint64_t>> legacyCursorFormats = {{DRM_FORMAT_ARGB8888, implicitModifier}};

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
    , m_commitThread(std::make_unique<DrmCommitThread>(conn->gpu(), conn->connectorName()))
{
}

DrmPipeline::~DrmPipeline()
{
}

DrmPipeline::Error DrmPipeline::present(const std::shared_ptr<OutputFrame> &frame)
{
    Q_ASSERT(m_pending.crtc);
    if (gpu()->atomicModeSetting()) {
        // test the full state, to take pending commits into account
        if (auto err = DrmPipeline::commitPipelinesAtomic({this}, CommitMode::Test, frame, {}); err != Error::None) {
            return err;
        }
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto primaryPlaneUpdate = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        if (Error err = prepareAtomicPresentation(primaryPlaneUpdate.get(), frame); err != Error::None) {
            return err;
        }
        if (m_pending.needsModesetProperties && !prepareAtomicModeset(primaryPlaneUpdate.get())) {
            return Error::InvalidArguments;
        }
        m_next.needsModesetProperties = m_pending.needsModesetProperties = false;
        m_commitThread->addCommit(std::move(primaryPlaneUpdate));
        return Error::None;
    } else {
        return presentLegacy(frame);
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
    for (const auto &pipeline : pipelines) {
        if (Error err = pipeline->prepareAtomicCommit(commit.get(), mode, frame); err != Error::None) {
            return err;
        }
    }
    for (const auto &unused : unusedObjects) {
        unused->disable(commit.get());
    }
    switch (mode) {
    case CommitMode::TestAllowModeset: {
        if (!commit->testAllowModeset()) {
            qCDebug(KWIN_DRM) << "Atomic modeset test failed!" << strerror(errno);
            return errnoToError();
        }
        const bool withoutModeset = std::ranges::all_of(pipelines, [&frame](DrmPipeline *pipeline) {
            auto commit = std::make_unique<DrmAtomicCommit>(QVector<DrmPipeline *>{pipeline});
            return pipeline->prepareAtomicCommit(commit.get(), CommitMode::TestAllowModeset, frame) == Error::None && commit->test();
        });
        for (const auto &pipeline : pipelines) {
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
            qCDebug(KWIN_DRM) << "Atomic test failed!" << strerror(errno);
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
        if (m_pending.crtc->cursorPlane()) {
            prepareAtomicCursor(commit);
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

    if (m_cursorLayer->isEnabled() && m_primaryLayer->colorPipeline() != m_cursorLayer->colorPipeline()) {
        return DrmPipeline::Error::InvalidArguments;
    }
    const ColorPipeline colorPipeline = m_primaryLayer->colorPipeline().merged(m_pending.crtcColorPipeline);
    if (!m_pending.crtc->postBlendingPipeline) {
        if (!colorPipeline.isIdentity()) {
            return Error::InvalidArguments;
        }
    } else {
        if (!m_pending.crtc->postBlendingPipeline->matchPipeline(commit, colorPipeline)) {
            return Error::InvalidArguments;
        }
    }

    if (!m_primaryLayer->checkTestBuffer()) {
        qCWarning(KWIN_DRM) << "Checking test buffer failed!";
        return Error::TestBufferFailed;
    }
    const auto fb = m_primaryLayer->currentBuffer();
    if (!fb) {
        return Error::InvalidArguments;
    }
    const auto primary = m_pending.crtc->primaryPlane();
    const auto transform = m_primaryLayer->offloadTransform();
    const auto planeTransform = DrmPlane::outputTransformToPlaneTransform(transform);
    if (primary->rotation.isValid()) {
        if (!primary->rotation.hasEnum(planeTransform)) {
            return Error::InvalidArguments;
        }
        commit->addEnum(primary->rotation, planeTransform);
    } else if (planeTransform != DrmPlane::Transformation::Rotate0) {
        return Error::InvalidArguments;
    }
    primary->set(commit, m_primaryLayer->sourceRect().toRect(), m_primaryLayer->targetRect());
    commit->addBuffer(m_pending.crtc->primaryPlane(), fb, frame);
    if (fb->buffer()->dmabufAttributes()->format == DRM_FORMAT_NV12) {
        if (!primary->colorEncoding.isValid() || !primary->colorRange.isValid()) {
            // don't allow NV12 direct scanout if we don't know what the driver will do
            return Error::InvalidArguments;
        }
        commit->addEnum(primary->colorEncoding, DrmPlane::ColorEncoding::BT709_YCbCr);
        commit->addEnum(primary->colorRange, DrmPlane::ColorRange::Limited_YCbCr);
    }
    return Error::None;
}

void DrmPipeline::prepareAtomicCursor(DrmAtomicCommit *commit)
{
    auto plane = m_pending.crtc->cursorPlane();
    const auto layer = cursorLayer();
    if (layer->isEnabled()) {
        plane->set(commit, layer->sourceRect().toRect(), layer->targetRect());
        commit->addProperty(plane->crtcId, m_pending.crtc->id());
        commit->addBuffer(plane, layer->currentBuffer(), nullptr);
        if (plane->vmHotspotX.isValid() && plane->vmHotspotY.isValid()) {
            commit->addProperty(plane->vmHotspotX, std::round(layer->hotspot().x()));
            commit->addProperty(plane->vmHotspotY, std::round(layer->hotspot().y()));
        }
    } else {
        commit->addProperty(plane->crtcId, 0);
        commit->addBuffer(plane, nullptr, nullptr);
    }
}

void DrmPipeline::prepareAtomicDisable(DrmAtomicCommit *commit)
{
    m_connector->disable(commit);
    if (m_pending.crtc) {
        m_pending.crtc->disable(commit);
        m_pending.crtc->primaryPlane()->disable(commit);
        if (auto cursor = m_pending.crtc->cursorPlane()) {
            cursor->disable(commit);
        }
    }
}

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
        if (m_connector->isInternal() && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::Full_Aspect) && (m_pending.mode->flags() & OutputMode::Flag::Generated)) {
            commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::Full_Aspect);
        } else if (m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
            commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::None);
        }
    }

    commit->addProperty(m_pending.crtc->active, 1);
    commit->addBlob(m_pending.crtc->modeId, m_pending.mode->blob());

    const auto primary = m_pending.crtc->primaryPlane();
    commit->addProperty(primary->crtcId, m_pending.crtc->id());
    if (primary->rotation.isValid()) {
        commit->addEnum(primary->rotation, {DrmPlane::Transformation::Rotate0});
    }
    if (primary->alpha.isValid()) {
        commit->addProperty(primary->alpha, primary->alpha.maxValue());
    }
    if (primary->pixelBlendMode.isValid()) {
        commit->addEnum(primary->pixelBlendMode, DrmPlane::PixelBlendMode::PreMultiplied);
    }
    if (const auto cursor = m_pending.crtc->cursorPlane()) {
        if (cursor->rotation.isValid()) {
            commit->addEnum(cursor->rotation, DrmPlane::Transformations(DrmPlane::Transformation::Rotate0));
        }
        if (cursor->alpha.isValid()) {
            commit->addProperty(cursor->alpha, cursor->alpha.maxValue());
        }
        if (cursor->pixelBlendMode.isValid()) {
            commit->addEnum(cursor->pixelBlendMode, DrmPlane::PixelBlendMode::PreMultiplied);
        }
        prepareAtomicCursor(commit);
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

bool DrmPipeline::updateCursor(std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    if (needsModeset() || !m_pending.crtc || !m_pending.active) {
        return false;
    }
    if (amdgpuVrrWorkaroundActive() && m_cursorLayer->isEnabled()) {
        return false;
    }
    // We need to make sure that on vmwgfx software cursor is selected
    // until Broadcom fixes hw cursor issues with vmwgfx. Otherwise
    // the cursor is missing.
    if (gpu()->isVmwgfx()) {
        return false;
    }
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        // test the full state, to take pending commits into account
        if (DrmPipeline::commitPipelinesAtomic({this}, CommitMode::Test, nullptr, {}) != Error::None) {
            return false;
        }
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto cursorOnly = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        prepareAtomicCursor(cursorOnly.get());
        cursorOnly->setAllowedVrrDelay(allowedVrrDelay);
        m_commitThread->addCommit(std::move(cursorOnly));
        return true;
    } else {
        return setCursorLegacy();
    }
}

bool DrmPipeline::amdgpuVrrWorkaroundActive() const
{
    static const bool s_env = qEnvironmentVariableIntValue("KWIN_DRM_DONT_FORCE_AMD_SW_CURSOR") == 1;
    return !s_env && gpu()->isAmdgpu() && (m_pending.presentationMode == PresentationMode::AdaptiveSync || m_pending.presentationMode == PresentationMode::AdaptiveAsync);
}

void DrmPipeline::applyPendingChanges()
{
    m_next = m_pending;
    m_commitThread->setModeInfo(m_pending.mode->refreshRate(), m_pending.mode->vblankTime());
    if (m_output) {
        m_output->renderLoop()->setPresentationSafetyMargin(m_commitThread->safetyMargin());
        m_output->renderLoop()->setRefreshRate(m_pending.mode->refreshRate());
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
    if (gpu()->needsModeset()) {
        gpu()->maybeModeset(nullptr, nullptr);
    }
}

void DrmPipeline::setOutput(DrmOutput *output)
{
    m_output = output;
}

DrmOutput *DrmPipeline::output() const
{
    return m_output;
}

QHash<uint32_t, QList<uint64_t>> DrmPipeline::formats(DrmPlane::TypeIndex planeType) const
{
    switch (planeType) {
    case DrmPlane::TypeIndex::Primary:
        return m_pending.formats;
    case DrmPlane::TypeIndex::Cursor:
        if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
            return m_pending.crtc->cursorPlane()->formats();
        } else {
            return legacyCursorFormats;
        }
    case DrmPlane::TypeIndex::Overlay:
        return {};
    }
    Q_UNREACHABLE();
}

bool DrmPipeline::pruneModifier()
{
    const DmaBufAttributes *dmabufAttributes = m_primaryLayer->currentBuffer() ? m_primaryLayer->currentBuffer()->buffer()->dmabufAttributes() : nullptr;
    if (!dmabufAttributes) {
        return false;
    }
    auto &modifiers = m_pending.formats[dmabufAttributes->format];
    if (modifiers == implicitModifier) {
        return false;
    } else {
        modifiers = implicitModifier;
        return true;
    }
}

QList<QSize> DrmPipeline::recommendedSizes(DrmPlane::TypeIndex planeType) const
{
    switch (planeType) {
    case DrmPlane::TypeIndex::Primary:
        if (m_pending.crtc && m_pending.crtc->primaryPlane()) {
            return m_pending.crtc->primaryPlane()->recommendedSizes();
        } else {
            return QList<QSize>{};
        }
    case DrmPlane::TypeIndex::Cursor:
        if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
            return m_pending.crtc->cursorPlane()->recommendedSizes();
        } else {
            return QList<QSize>{gpu()->cursorSize()};
        }
    case DrmPlane::TypeIndex::Overlay:
        return QList<QSize>{};
    }
    Q_UNREACHABLE();
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

DrmPipelineLayer *DrmPipeline::primaryLayer() const
{
    return m_primaryLayer.get();
}

DrmPipelineLayer *DrmPipeline::cursorLayer() const
{
    return m_cursorLayer.get();
}

PresentationMode DrmPipeline::presentationMode() const
{
    return m_pending.presentationMode;
}

uint32_t DrmPipeline::overscan() const
{
    return m_pending.overscan;
}

Output::RgbRange DrmPipeline::rgbRange() const
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
    if (crtc) {
        m_pending.formats = crtc->primaryPlane() ? crtc->primaryPlane()->formats() : legacyFormats;
    } else {
        m_pending.formats = {};
    }
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

void DrmPipeline::setLayers(const std::shared_ptr<DrmPipelineLayer> &primaryLayer, const std::shared_ptr<DrmPipelineLayer> &cursorLayer)
{
    m_primaryLayer = primaryLayer;
    m_cursorLayer = cursorLayer;
}

void DrmPipeline::setPresentationMode(PresentationMode mode)
{
    m_pending.presentationMode = mode;
}

void DrmPipeline::setOverscan(uint32_t overscan)
{
    m_pending.overscan = overscan;
}

void DrmPipeline::setRgbRange(Output::RgbRange range)
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
    const auto colorimetry = m_connector->edid()->colorimetry().value_or(Colorimetry::BT709);
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
