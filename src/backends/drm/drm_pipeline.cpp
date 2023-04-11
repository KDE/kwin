/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "core/session.h"
#include "drm_atomic_commit.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_buffer_gbm.h"
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

namespace KWin
{

static const QMap<uint32_t, QVector<uint64_t>> legacyFormats = {{DRM_FORMAT_XRGB8888, {}}};
static const QMap<uint32_t, QVector<uint64_t>> legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {}}};

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
{
}

DrmPipeline::~DrmPipeline()
{
    if (m_pageflipPending && m_current.crtc) {
        pageFlipped({});
    }
}

bool DrmPipeline::testScanout()
{
    // TODO make the modeset check only be tested at most once per scanout cycle
    if (gpu()->needsModeset()) {
        return false;
    }
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Test) == Error::None;
    } else {
        if (m_pending.layer->currentBuffer()->buffer()->size() != m_pending.mode->size()) {
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
        return commitPipelines({this}, CommitMode::Commit);
    } else {
        if (m_pending.layer->hasDirectScanoutBuffer()) {
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

DrmPipeline::Error DrmPipeline::commitPipelines(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode);
    }
}

DrmPipeline::Error DrmPipeline::commitPipelinesAtomic(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    auto commit = std::make_unique<DrmAtomicCommit>(pipelines.front()->gpu());
    for (const auto &pipeline : pipelines) {
        if (pipeline->activePending()) {
            if (!pipeline->m_pending.layer->checkTestBuffer()) {
                qCWarning(KWIN_DRM) << "Checking test buffer failed for" << mode;
                return Error::TestBufferFailed;
            }
            if (!pipeline->prepareAtomicPresentation(commit.get())) {
                return Error::InvalidArguments;
            }
            if (mode == CommitMode::TestAllowModeset || mode == CommitMode::CommitModeset) {
                pipeline->prepareAtomicModeset(commit.get());
            }
        } else {
            pipeline->prepareAtomicDisable(commit.get());
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
        const bool withoutModeset = commit->test();
        for (const auto &pipeline : pipelines) {
            pipeline->m_pending.needsModeset = !withoutModeset;
        }
        return Error::None;
    }
    case CommitMode::CommitModeset: {
        // The kernel fails commits with DRM_MODE_PAGE_FLIP_EVENT when a crtc is disabled in the commit
        // and already was disabled before, to work around some quirks in old userspace.
        // Instead of using DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, do the modeset in a blocking
        // fashion without page flip events and directly call the pageFlipped method afterwards
        if (!commit->commitModeset()) {
            qCCritical(KWIN_DRM) << "Atomic modeset commit failed!" << strerror(errno);
            return errnoToError();
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicModesetSuccessful));
        for (const auto &obj : unusedObjects) {
            if (auto crtc = dynamic_cast<DrmCrtc *>(obj)) {
                crtc->flipBuffer();
            } else if (auto plane = dynamic_cast<DrmPlane *>(obj)) {
                plane->flipBuffer();
            }
        }
        return Error::None;
    }
    case CommitMode::Test: {
        if (!commit->test()) {
            qCDebug(KWIN_DRM) << "Atomic test failed!" << strerror(errno);
            return errnoToError();
        }
        return Error::None;
    }
    case CommitMode::Commit: {
        if (!commit->commit()) {
            qCCritical(KWIN_DRM) << "Atomic commit failed!" << strerror(errno);
            return errnoToError();
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicCommitSuccessful));
        Q_ASSERT(unusedObjects.isEmpty());
        return Error::None;
    }
    default:
        Q_UNREACHABLE();
    }
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

bool DrmPipeline::prepareAtomicPresentation(DrmAtomicCommit *commit)
{
    if (m_connector->contentType.isValid()) {
        commit->addEnum(m_connector->contentType, m_pending.contentType);
    }

    commit->addProperty(m_pending.crtc->vrrEnabled, m_pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive || m_pending.syncMode == RenderLoopPrivate::SyncMode::AdaptiveAsync);
    if (m_pending.crtc->gammaLut.isValid()) {
        commit->addBlob(m_pending.crtc->gammaLut, m_pending.gamma ? m_pending.gamma->blob() : nullptr);
    } else if (m_pending.gamma) {
        return false;
    }
    if (m_pending.crtc->ctm.isValid()) {
        commit->addBlob(m_pending.crtc->ctm, m_pending.ctm);
    } else if (m_pending.ctm) {
        return false;
    }

    const auto fb = m_pending.layer->currentBuffer().get();
    m_pending.crtc->primaryPlane()->set(commit, QPoint(0, 0), fb->buffer()->size(), centerBuffer(fb->buffer()->size(), m_pending.mode->size()));
    commit->addProperty(m_pending.crtc->primaryPlane()->fbId, fb->framebufferId());

    if (auto plane = m_pending.crtc->cursorPlane()) {
        const auto layer = cursorLayer();
        plane->set(commit, QPoint(0, 0), gpu()->cursorSize(), QRect(layer->position(), gpu()->cursorSize()));
        commit->addProperty(plane->crtcId, layer->isVisible() ? m_pending.crtc->id() : 0);
        commit->addProperty(plane->fbId, layer->isVisible() ? layer->currentBuffer()->framebufferId() : 0);
    }
    return true;
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

void DrmPipeline::prepareAtomicModeset(DrmAtomicCommit *commit)
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
        commit->addProperty(m_connector->hdrMetadata, 0);
    }
    if (m_connector->scalingMode.isValid() && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
        commit->addEnum(m_connector->scalingMode, DrmConnector::ScalingMode::None);
    }

    commit->addProperty(m_pending.crtc->active, 1);
    commit->addBlob(m_pending.crtc->modeId, m_pending.mode->blob());

    commit->addProperty(m_pending.crtc->primaryPlane()->crtcId, m_pending.crtc->id());
    if (const auto &rotation = m_pending.crtc->primaryPlane()->rotation; rotation.isValid()) {
        commit->addEnum(rotation, {DrmPlane::Transformation::Rotate0});
    }
    if (m_pending.crtc->cursorPlane()) {
        if (const auto &rotation = m_pending.crtc->cursorPlane()->rotation; rotation.isValid()) {
            commit->addEnum(rotation, DrmPlane::Transformations(DrmPlane::Transformation::Rotate0));
        }
    }
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

void DrmPipeline::atomicCommitSuccessful()
{
    m_pending.needsModeset = false;
    if (activePending()) {
        m_pageflipPending = true;
    }
    if (m_pending.crtc) {
        m_pending.crtc->primaryPlane()->setNext(m_pending.layer->currentBuffer());
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->setNext(cursorLayer()->currentBuffer());
        }
    }
    m_current = m_pending;
}

void DrmPipeline::atomicModesetSuccessful()
{
    atomicCommitSuccessful();
    m_pending.needsModeset = false;
    if (activePending()) {
        pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
    }
}

bool DrmPipeline::setCursor(const QPoint &hotspot)
{
    bool result;
    m_pending.cursorHotspot = hotspot;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test) == Error::None;
        if (result && m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        result = setCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
    } else {
        m_pending = m_next;
    }
    return result;
}

bool DrmPipeline::moveCursor()
{
    bool result;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test) == Error::None;
    } else {
        result = moveCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
        if (m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        m_pending = m_next;
    }
    return result;
}

void DrmPipeline::applyPendingChanges()
{
    m_next = m_pending;
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
    m_current.crtc->flipBuffer();
    if (m_current.crtc->primaryPlane()) {
        m_current.crtc->primaryPlane()->flipBuffer();
    }
    if (m_current.crtc->cursorPlane()) {
        m_current.crtc->cursorPlane()->flipBuffer();
    }
    m_pageflipPending = false;
    if (m_output) {
        m_output->pageFlipped(timestamp);
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

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::formats() const
{
    return m_pending.formats;
}

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::cursorFormats() const
{
    if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
        return m_pending.crtc->cursorPlane()->formats();
    } else {
        return legacyCursorFormats;
    }
}

bool DrmPipeline::pruneModifier()
{
    if (!m_pending.layer->currentBuffer()
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_NONE
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_INVALID) {
        return false;
    }
    auto &modifiers = m_pending.formats[m_pending.layer->currentBuffer()->buffer()->format()];
    if (modifiers.empty()) {
        return false;
    } else {
        modifiers.clear();
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

bool DrmPipeline::pageflipPending() const
{
    return m_pageflipPending;
}

bool DrmPipeline::modesetPresentPending() const
{
    return m_modesetPresentPending;
}

void DrmPipeline::resetModesetPresentPending()
{
    m_modesetPresentPending = false;
}

DrmCrtc *DrmPipeline::currentCrtc() const
{
    return m_current.crtc;
}

DrmGammaRamp::DrmGammaRamp(DrmCrtc *crtc, const std::shared_ptr<ColorTransformation> &transformation)
    : m_lut(transformation, crtc->gammaRampSize())
{
    if (crtc->gpu()->atomicModeSetting()) {
        QVector<drm_color_lut> atomicLut(m_lut.size());
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
    return m_pending.layer.get();
}

DrmOverlayLayer *DrmPipeline::cursorLayer() const
{
    return m_pending.cursorLayer.get();
}

DrmPlane::Transformations DrmPipeline::renderOrientation() const
{
    return m_pending.renderOrientation;
}

RenderLoopPrivate::SyncMode DrmPipeline::syncMode() const
{
    return m_pending.syncMode;
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

void DrmPipeline::setLayers(const std::shared_ptr<DrmPipelineLayer> &primaryLayer, const std::shared_ptr<DrmOverlayLayer> &cursorLayer)
{
    m_pending.layer = primaryLayer;
    m_pending.cursorLayer = cursorLayer;
}

void DrmPipeline::setRenderOrientation(DrmPlane::Transformations orientation)
{
    m_pending.renderOrientation = orientation;
}

void DrmPipeline::setSyncMode(RenderLoopPrivate::SyncMode mode)
{
    m_pending.syncMode = mode;
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
    uint64_t ret = std::abs(value) * (1ul << 32);
    if (value < 0) {
        ret |= 1ul << 63;
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
}
