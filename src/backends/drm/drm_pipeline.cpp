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
static const QMap<uint32_t, QList<uint64_t>> legacyFormats = {{DRM_FORMAT_XRGB8888, implicitModifier}};
static const QMap<uint32_t, QList<uint64_t>> legacyCursorFormats = {{DRM_FORMAT_ARGB8888, implicitModifier}};

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
    , m_commitThread(std::make_unique<DrmCommitThread>(conn->connectorName()))
{
    QObject::connect(m_commitThread.get(), &DrmCommitThread::commitFailed, [this]() {
        if (m_output) {
            m_output->frameFailed();
        }
    });
}

DrmPipeline::~DrmPipeline()
{
    if (pageflipsPending()) {
        gpu()->waitIdle();
    }
}

bool DrmPipeline::testScanout()
{
    if (gpu()->needsModeset()) {
        return false;
    }
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Test) == Error::None;
    } else {
        if (m_primaryLayer->currentBuffer()->buffer()->size() != m_pending.mode->size()) {
            // scaling isn't supported with the legacy API
            return false;
        }
        // no other way to test than to do it.
        // As we only have a maximum of one test per scanout cycle, this is fine
        return presentLegacy() == Error::None;
    }
}

DrmPipeline::Error DrmPipeline::present()
{
    Q_ASSERT(m_pending.crtc);
    if (gpu()->atomicModeSetting()) {
        // test the full state, to take pending commits into account
        auto fullState = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        if (Error err = prepareAtomicCommit(fullState.get(), CommitMode::Test); err != Error::None) {
            return err;
        }
        if (!fullState->test()) {
            return errnoToError();
        }
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto primaryPlaneUpdate = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        if (Error err = prepareAtomicPresentation(primaryPlaneUpdate.get()); err != Error::None) {
            return err;
        }
        if (m_pending.needsModesetProperties && !prepareAtomicModeset(primaryPlaneUpdate.get())) {
            return Error::InvalidArguments;
        }
        m_next.needsModesetProperties = m_pending.needsModesetProperties = false;
        m_commitThread->addCommit(std::move(primaryPlaneUpdate));
        return Error::None;
    } else {
        if (m_primaryLayer->hasDirectScanoutBuffer()) {
            // already presented
            return Error::None;
        }
        return presentLegacy();
    }
}

bool DrmPipeline::maybeModeset()
{
    m_modesetPresentPending = true;
    return gpu()->maybeModeset();
}

DrmPipeline::Error DrmPipeline::commitPipelines(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode);
    }
}

DrmPipeline::Error DrmPipeline::commitPipelinesAtomic(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    auto commit = std::make_unique<DrmAtomicCommit>(pipelines);
    if (mode == CommitMode::Test) {
        // if there's a modeset pending, the tests on top of that state
        // also have to allow modesets or they'll always fail
        const bool wantsModeset = std::any_of(pipelines.begin(), pipelines.end(), [](DrmPipeline *pipeline) {
            return pipeline->needsModeset();
        });
        if (wantsModeset) {
            mode = CommitMode::TestAllowModeset;
        }
    }
    for (const auto &pipeline : pipelines) {
        if (Error err = pipeline->prepareAtomicCommit(commit.get(), mode); err != Error::None) {
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
        const bool withoutModeset = std::all_of(pipelines.begin(), pipelines.end(), [](DrmPipeline *pipeline) {
            auto commit = std::make_unique<DrmAtomicCommit>(QVector<DrmPipeline *>{pipeline});
            return pipeline->prepareAtomicCommit(commit.get(), CommitMode::TestAllowModeset) == Error::None && commit->test();
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

DrmPipeline::Error DrmPipeline::prepareAtomicCommit(DrmAtomicCommit *commit, CommitMode mode)
{
    if (activePending()) {
        if (Error err = prepareAtomicPresentation(commit); err != Error::None) {
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

static QRect centerBuffer(const QSize &bufferSize, const QSize &modeSize)
{
    const double widthScale = bufferSize.width() / double(modeSize.width());
    const double heightScale = bufferSize.height() / double(modeSize.height());
    if (widthScale > heightScale) {
        const QSize size = bufferSize / widthScale;
        const uint32_t yOffset = (modeSize.height() - size.height()) / 2;
        return QRect(QPoint(0, yOffset), size);
    } else {
        const QSize size = bufferSize / heightScale;
        const uint32_t xOffset = (modeSize.width() - size.width()) / 2;
        return QRect(QPoint(xOffset, 0), size);
    }
}

DrmPipeline::Error DrmPipeline::prepareAtomicPresentation(DrmAtomicCommit *commit)
{
    commit->setPresentationMode(m_pending.presentationMode);
    if (m_connector->contentType.isValid()) {
        commit->addEnum(m_connector->contentType, m_pending.contentType);
    }

    if (m_pending.crtc->vrrEnabled.isValid()) {
        commit->setVrr(m_pending.crtc, m_pending.presentationMode == PresentationMode::AdaptiveSync || m_pending.presentationMode == PresentationMode::AdaptiveAsync);
    }
    if (m_pending.crtc->gammaLut.isValid()) {
        commit->addBlob(m_pending.crtc->gammaLut, m_pending.gamma ? m_pending.gamma->blob() : nullptr);
    } else if (m_pending.gamma) {
        return Error::InvalidArguments;
    }
    if (m_pending.crtc->ctm.isValid()) {
        commit->addBlob(m_pending.crtc->ctm, m_pending.ctm);
    } else if (m_pending.ctm) {
        return Error::InvalidArguments;
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
    primary->set(commit, QPoint(0, 0), fb->buffer()->size(), centerBuffer(fb->buffer()->size(), m_pending.mode->size()));
    commit->addBuffer(m_pending.crtc->primaryPlane(), fb);
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
    plane->set(commit, QPoint(0, 0), gpu()->cursorSize(), QRect(layer->position().toPoint(), gpu()->cursorSize()));
    commit->addProperty(plane->crtcId, layer->isEnabled() ? m_pending.crtc->id() : 0);
    commit->addBuffer(plane, layer->isEnabled() ? layer->currentBuffer() : nullptr);
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
        uint64_t preferred = 8;
        if (auto backend = dynamic_cast<EglGbmBackend *>(gpu()->platform()->renderBackend()); backend && backend->prefer10bpc()) {
            preferred = 10;
        }
        commit->addProperty(m_connector->maxBpc, preferred);
    }
    if (m_connector->hdrMetadata.isValid()) {
        commit->addBlob(m_connector->hdrMetadata, createHdrMetadata(m_pending.transferFunction));
    } else if (m_pending.transferFunction != NamedTransferFunction::sRGB) {
        return false;
    }
    if (m_pending.BT2020) {
        if (!m_connector->colorspace.isValid() || !m_connector->colorspace.hasEnum(DrmConnector::Colorspace::BT2020_RGB)) {
            return false;
        }
        commit->addEnum(m_connector->colorspace, DrmConnector::Colorspace::BT2020_RGB);
    } else if (m_connector->colorspace.isValid()) {
        commit->addEnum(m_connector->colorspace, DrmConnector::Colorspace::Default);
    }
    if (m_connector->scalingMode.isValid() && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
        commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::None);
    }

    commit->addProperty(m_pending.crtc->active, 1);
    commit->addBlob(m_pending.crtc->modeId, m_pending.mode->blob());
    if (m_pending.crtc->degammaLut.isValid()) {
        commit->addBlob(m_pending.crtc->degammaLut, nullptr);
    }

    const auto primary = m_pending.crtc->primaryPlane();
    commit->addProperty(primary->crtcId, m_pending.crtc->id());
    if (primary->rotation.isValid()) {
        commit->addEnum(primary->rotation, {DrmPlane::Transformation::Rotate0});
    }
    if (primary->alpha.isValid()) {
        commit->addProperty(primary->alpha, 0xFFFF);
    }
    if (primary->pixelBlendMode.isValid()) {
        commit->addEnum(primary->pixelBlendMode, DrmPlane::PixelBlendMode::PreMultiplied);
    }
    if (const auto cursor = m_pending.crtc->cursorPlane()) {
        if (cursor->rotation.isValid()) {
            commit->addEnum(cursor->rotation, DrmPlane::Transformations(DrmPlane::Transformation::Rotate0));
        }
        if (cursor->alpha.isValid()) {
            commit->addProperty(cursor->alpha, 0xFFFF);
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

bool DrmPipeline::updateCursor()
{
    if (needsModeset() || !m_pending.crtc || !m_pending.active) {
        return false;
    }
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        // test the full state, to take pending commits into account
        auto fullState = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        if (prepareAtomicPresentation(fullState.get()) != Error::None) {
            return false;
        }
        prepareAtomicCursor(fullState.get());
        if (!fullState->test()) {
            return false;
        }
        // only give the actual state update to the commit thread, so that it can potentially reorder the commits
        auto cursorOnly = std::make_unique<DrmAtomicCommit>(QList<DrmPipeline *>{this});
        prepareAtomicCursor(cursorOnly.get());
        cursorOnly->setCursorOnly(true);
        m_commitThread->addCommit(std::move(cursorOnly));
        return true;
    } else {
        return setCursorLegacy();
    }
}

void DrmPipeline::applyPendingChanges()
{
    m_next = m_pending;
    m_commitThread->setRefreshRate(m_pending.mode->refreshRate());
}

DrmConnector *DrmPipeline::connector() const
{
    return m_connector;
}

DrmGpu *DrmPipeline::gpu() const
{
    return m_connector->gpu();
}

void DrmPipeline::pageFlipped(std::chrono::nanoseconds timestamp, PageflipType type, PresentationMode mode)
{
    m_commitThread->pageFlipped(timestamp);
    m_legacyPageflipPending = false;
    if (type == PageflipType::Modeset && !activePending()) {
        return;
    }
    if (m_output) {
        if (type == PageflipType::Normal || type == PageflipType::Modeset) {
            m_output->pageFlipped(timestamp, mode);
        } else {
            RenderLoopPrivate::get(m_output->renderLoop())->notifyVblank(timestamp);
        }
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

QMap<uint32_t, QList<uint64_t>> DrmPipeline::formats() const
{
    return m_pending.formats;
}

QMap<uint32_t, QList<uint64_t>> DrmPipeline::cursorFormats() const
{
    if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
        return m_pending.crtc->cursorPlane()->formats();
    } else {
        return legacyCursorFormats;
    }
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

bool DrmPipeline::pageflipsPending() const
{
    return gpu()->atomicModeSetting() ? m_commitThread->pageflipsPending() : m_legacyPageflipPending;
}

bool DrmPipeline::modesetPresentPending() const
{
    return m_modesetPresentPending;
}

void DrmPipeline::resetModesetPresentPending()
{
    m_modesetPresentPending = false;
}

DrmGammaRamp::DrmGammaRamp(DrmCrtc *crtc, const std::shared_ptr<ColorTransformation> &transformation)
    : m_lut(transformation, crtc->gammaRampSize())
{
    if (crtc->gpu()->atomicModeSetting()) {
        QList<drm_color_lut> atomicLut(m_lut.size());
        for (uint32_t i = 0; i < m_lut.size(); i++) {
            atomicLut[i].red = m_lut.red()[i];
            atomicLut[i].green = m_lut.green()[i];
            atomicLut[i].blue = m_lut.blue()[i];
        }
        m_blob = DrmBlob::create(crtc->gpu(), atomicLut.data(), sizeof(drm_color_lut) * atomicLut.size());
    }
}

const ColorLUT &DrmGammaRamp::lut() const
{
    return m_lut;
}

std::shared_ptr<DrmBlob> DrmGammaRamp::blob() const
{
    return m_blob;
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

DrmPlane::Transformations DrmPipeline::renderOrientation() const
{
    return m_pending.renderOrientation;
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

const ColorDescription &DrmPipeline::colorDescription() const
{
    return m_pending.colorDescription;
}

const std::shared_ptr<IccProfile> &DrmPipeline::iccProfile() const
{
    return m_pending.iccProfile;
}

void DrmPipeline::setCrtc(DrmCrtc *crtc)
{
    if (crtc && m_pending.crtc && crtc->gammaRampSize() != m_pending.crtc->gammaRampSize() && m_pending.colorTransformation) {
        m_pending.gamma = std::make_shared<DrmGammaRamp>(crtc, m_pending.colorTransformation);
    }
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

void DrmPipeline::setRenderOrientation(DrmPlane::Transformations orientation)
{
    m_pending.renderOrientation = orientation;
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

void DrmPipeline::setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation)
{
    m_pending.colorTransformation = transformation;
    m_pending.gamma = std::make_shared<DrmGammaRamp>(m_pending.crtc, transformation);
}

static uint64_t doubleToFixed(double value)
{
    // ctm values are in S31.32 sign-magnitude format
    uint64_t ret = std::abs(value) * (1ull << 32);
    if (value < 0) {
        ret |= 1ull << 63;
    }
    return ret;
}

void DrmPipeline::setCTM(const QMatrix3x3 &ctm)
{
    if (ctm.isIdentity()) {
        m_pending.ctm.reset();
    } else {
        drm_color_ctm blob = {
            .matrix = {
                doubleToFixed(ctm(0, 0)), doubleToFixed(ctm(1, 0)), doubleToFixed(ctm(2, 0)),
                doubleToFixed(ctm(0, 1)), doubleToFixed(ctm(1, 1)), doubleToFixed(ctm(2, 1)),
                doubleToFixed(ctm(0, 2)), doubleToFixed(ctm(1, 2)), doubleToFixed(ctm(2, 2))},
        };
        m_pending.ctm = DrmBlob::create(gpu(), &blob, sizeof(blob));
    }
}

void DrmPipeline::setContentType(DrmConnector::DrmContentType type)
{
    m_pending.contentType = type;
}

void DrmPipeline::setBT2020(bool useBT2020)
{
    if (m_pending.BT2020 != useBT2020) {
        m_pending.BT2020 = useBT2020;
        m_pending.colorDescription = createColorDescription();
    }
}

void DrmPipeline::setNamedTransferFunction(NamedTransferFunction tf)
{
    if (m_pending.transferFunction != tf) {
        m_pending.transferFunction = tf;
        m_pending.colorDescription = createColorDescription();
    }
}

void DrmPipeline::setSdrBrightness(double sdrBrightness)
{
    if (m_pending.sdrBrightness != sdrBrightness) {
        m_pending.sdrBrightness = sdrBrightness;
        m_pending.colorDescription = createColorDescription();
    }
}

void DrmPipeline::setSdrGamutWideness(double sdrGamutWideness)
{
    if (m_pending.sdrGamutWideness != sdrGamutWideness) {
        m_pending.sdrGamutWideness = sdrGamutWideness;
        m_pending.colorDescription = createColorDescription();
    }
}

void DrmPipeline::setBrightnessOverrides(std::optional<double> peakBrightnessOverride, std::optional<double> averageBrightnessOverride, std::optional<double> minBrightnessOverride)
{
    if (m_pending.peakBrightnessOverride != peakBrightnessOverride || m_pending.averageBrightnessOverride != averageBrightnessOverride || m_pending.minBrightnessOverride != minBrightnessOverride) {
        m_pending.peakBrightnessOverride = peakBrightnessOverride;
        m_pending.averageBrightnessOverride = averageBrightnessOverride;
        m_pending.minBrightnessOverride = minBrightnessOverride;
        m_pending.colorDescription = createColorDescription();
    }
}

void DrmPipeline::setIccProfile(const std::shared_ptr<IccProfile> &profile)
{
    if (m_pending.iccProfile != profile) {
        m_pending.iccProfile = profile;
        m_pending.colorDescription = createColorDescription();
    }
}

ColorDescription DrmPipeline::createColorDescription() const
{
    if (m_pending.transferFunction == NamedTransferFunction::PerceptualQuantizer && m_connector->edid()) {
        const auto colorimetry = m_pending.BT2020 ? NamedColorimetry::BT2020 : NamedColorimetry::BT709;
        if (const auto hdr = m_connector->edid()->hdrMetadata(); hdr && hdr->hasValidBrightnessValues) {
            return ColorDescription(colorimetry, m_pending.transferFunction, m_pending.sdrBrightness, hdr->desiredContentMinLuminance, hdr->desiredMaxFrameAverageLuminance, hdr->desiredContentMaxLuminance, m_pending.sdrGamutWideness);
        } else if (m_pending.peakBrightnessOverride && m_pending.averageBrightnessOverride) {
            return ColorDescription(colorimetry, m_pending.transferFunction, m_pending.sdrBrightness, m_pending.minBrightnessOverride.value_or(0), *m_pending.averageBrightnessOverride, *m_pending.peakBrightnessOverride, m_pending.sdrGamutWideness);
        } else {
            return ColorDescription(colorimetry, m_pending.transferFunction, m_pending.sdrBrightness, 0, m_pending.sdrBrightness, m_pending.sdrBrightness, m_pending.sdrGamutWideness);
        }
    } else if (m_pending.iccProfile) {
        return ColorDescription(m_pending.iccProfile->colorimetry(), NamedTransferFunction::sRGB, 200, 0, 200, 200, 0);
    } else {
        return ColorDescription::sRGB;
    }
}

std::shared_ptr<DrmBlob> DrmPipeline::createHdrMetadata(NamedTransferFunction transferFunction) const
{
    if (transferFunction != NamedTransferFunction::PerceptualQuantizer) {
        // for sRGB / gamma 2.2, don't send any metadata, to ensure the non-HDR experience stays the same
        return nullptr;
    }
    if (!m_connector->edid() || !m_connector->edid()->hdrMetadata()) {
        return nullptr;
    }
    const auto metadata = *m_connector->edid()->hdrMetadata();
    if (!metadata.supportsPQ) {
        return nullptr;
    }
    const auto colorimetry = m_connector->edid()->colorimetry();
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
                {to16Bit(colorimetry.red.x()), to16Bit(colorimetry.red.y())},
                {to16Bit(colorimetry.green.x()), to16Bit(colorimetry.green.y())},
                {to16Bit(colorimetry.blue.x()), to16Bit(colorimetry.blue.y())},
            },
            .white_point = {to16Bit(colorimetry.white.x()), to16Bit(colorimetry.white.y())},
            // in nits
            .max_display_mastering_luminance = uint16_t(std::round(metadata.desiredMaxFrameAverageLuminance)),
            // in 0.0001 nits
            .min_display_mastering_luminance = uint16_t(std::round(metadata.desiredContentMinLuminance * 10000)),
            // in nits
            .max_cll = uint16_t(std::round(metadata.desiredMaxFrameAverageLuminance)),
            .max_fall = uint16_t(std::round(metadata.desiredMaxFrameAverageLuminance)),
        },
    };
    return DrmBlob::create(gpu(), &data, sizeof(data));
}
}
